#include "server.h"

#include <error.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <vector>
#include <thread>

#include "worker.h"

using namespace http;

server::server() noexcept
{
    _isRunning.store(false);
}

server::~server()
{
    stop();
}

bool server::start(const std::string &host,
                   uint16_t port,
                   std::function<void(request* request)> request_handler,
                   std::function<handle_res(request*, http::uri)> uri_handler,
                   std::function<handle_res(request*, http::string, http::string)> header_handler) noexcept
{
    if (!init(host, port)) {
        uninit();
        return false;
    }

    _request_handler = request_handler;
    _uri_handler = uri_handler;
    _header_handler = header_handler;

    _isRunning.store(true);
    _thread = std::thread(&server::loop, this);

    return true;
}

void server::stop() noexcept
{
    // TODO: uncomment after test
//    _isRunning.store(false);
    if (_thread.joinable()) {
        _thread.join();
    }

    uninit();
}

bool server::init(const std::string &host, uint16_t port) noexcept
{
    _sd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sd == -1) {
        perror("socket");
        return false;
    }

    in_addr addr;
    addr.s_addr = inet_addr(host.c_str());

    sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_addr = addr;
    in.sin_port = htons(port);

    if (bind(_sd, reinterpret_cast<sockaddr*>(&in), sizeof (in)) == -1) {
        perror("bind");
        return false;
    }

    if (listen(_sd, 1000) == -1) {
        perror("listen");
        return false;
    }

    const size_t epoll_num = 3;
    _epolls.reserve(epoll_num);
    for (size_t i=0; i<epoll_num; ++i) {
        int fd = epoll_create1(0);
        if (fd != -1) {
            _epolls.push_back(fd);
        } else {
            perror("epoll_create");
            return false;
        }
    }

    for (size_t i=0; i<_epolls.size(); ++i) {
        worker* wrk = new worker(_epolls.at(i));
        wrk->start();
        _workers.push_back(wrk);
    }

    return true;
}

void server::uninit() noexcept
{
    for (auto&& worker : _workers) {
        worker->stop();
        delete worker;
    }

    if (_sd != -1) {
        close(_sd);
    }
}

void server::loop() noexcept
{
    size_t i = 0;
    while (_isRunning) {
        int conn_fd = accept(_sd, nullptr, nullptr);
        if (conn_fd == -1) {
            perror("accept");
            continue;
        }

        request* req = new request;
        req->request_handler = _request_handler;
        req->uri_handler = _uri_handler;
        req->header_handler = _header_handler;
        req->sock_d = conn_fd;

        epoll_event in_event;
        in_event.events = EPOLLIN;
        in_event.data.ptr = req;

        if (epoll_ctl(_epolls.at(i), EPOLL_CTL_ADD, conn_fd, &in_event) == -1) {
            perror("epoll_ctl");
        }

        ++i;
        if (i+1 >= _epolls.size()) {
            i = 0;
        }
    }
}
