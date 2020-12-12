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

#include "server_worker.h"
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
                   in_request_handler request_handl,
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

    LOG(INFO) << "server listen at " << host << " " << port;

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

bool server::init(const std::string& host, uint16_t port) noexcept
{
    _listening_sock_d = socket(AF_INET, SOCK_STREAM, 0);
    if (_listening_sock_d == -1) {
        perror("socket");
        return false;
    }

    in_addr addr;
    addr.s_addr = inet_addr(host.c_str());

    sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_addr = addr;
    in.sin_port = htons(port);

    if (bind(_listening_sock_d, reinterpret_cast<sockaddr*>(&in), sizeof (in)) == -1) {
        perror("bind");
        return false;
    }

    if (listen(_listening_sock_d, 1000) == -1) {
        perror("listen");
        return false;
    }

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
        server_worker* wrk = new server_worker(_epolls.at(i));
        wrk->start();
        _workers.push_back(wrk);
    }

    return true;
}

void server::uninit() noexcept
{
    for (auto worker : _workers) {
        worker->stop();
        delete worker;
    }
    _workers.clear();

    for (int epoll : _epolls) {
        close(epoll);
    }
    _epolls.clear();

    if (_listening_sock_d != -1) {
        close(_listening_sock_d);
    }
}

void server::loop() noexcept
{
    sockaddr_storage client_addr;
    socklen_t client_addr_len = 0;
    char client_addr_str[INET6_ADDRSTRLEN];

    size_t i = 0;
    while (_isRunning) {
        client_addr_len = sizeof (client_addr);
        int conn_fd = accept(_listening_sock_d, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
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
        conn->in_req_handler = _request_handler;
        conn->req_state_machine = std::make_unique<request_state_machine>(_uri_handler, _header_handler);

        if (client_addr.ss_family == AF_INET) {
            const sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&client_addr);
            conn->remote_port = ntohs(addr->sin_port);
            conn->remote_host = std::string(inet_ntop(AF_INET, &addr->sin_addr, client_addr_str, sizeof(client_addr_str)));
        } else if (client_addr.ss_family == AF_INET6) {
            const sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(&client_addr);
            conn->remote_port = ntohs(addr->sin6_port);
            conn->remote_host = std::string(inet_ntop(AF_INET, &addr->sin6_addr, client_addr_str, sizeof(client_addr_str)));
        }

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
