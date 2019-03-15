#include "server.h"

#include <error.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include <vector>
#include <thread>

#include <glog/logging.h>

#include "worker.h"
#include "request_state_machine.h"

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
                   request_handler request_handl,
                   uri_handler uri_hand,
                   header_handler header_hand) noexcept
{
    if (!init(host, port)) {
        uninit();
        return false;
    }

    _request_handler = request_handl;
    _uri_handler = uri_hand;
    _header_handler = header_hand;

    _isRunning.store(true);
    _thread = std::thread(&server::loop, this);

    return true;
}

void server::stop() noexcept
{
    _isRunning.store(false);
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

    LOG(INFO) << "Listen " << host << " " << port;

    const size_t epoll_num = 1;
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

        if (fcntl(conn_fd, F_SETFL, O_NONBLOCK) != 0) {
            perror("fcntl to NONBLOCK");
            continue;
        }

        connection* conn = new connection;
        conn->sock_d = conn_fd;
        conn->req_handler = _request_handler;
        conn->req_state_machine = std::make_unique<request_state_machine>(_uri_handler, _header_handler);

        epoll_event in_event;
        in_event.events = EPOLLIN | EPOLLRDHUP;
        in_event.data.ptr = conn;

        if (epoll_ctl(_epolls.at(i), EPOLL_CTL_ADD, conn_fd, &in_event) == -1) {
            perror("epoll_ctl");
        }

        ++i;
        if (i >= _epolls.size()) {
            i = 0;
        }
    }
}
