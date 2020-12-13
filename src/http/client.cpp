#include "client.h"

#include <error.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <cstring>

#include <glog/logging.h>

#include "connection.h"
#include "client_worker.h"

using namespace http;

client::client()
{
    try {
        init();
    } catch (const std::exception& e) {
        uninit();
        throw e;
    }
}

client::~client()
{
    uninit();
}

void client::init()
{
    // TODO: refactoring: move managing of epoll to worker and
    // create and use method worker::add_connection(connection* conn)

    const size_t epoll_num = 1;
    _epolls.reserve(epoll_num);
    for (size_t i=0; i<epoll_num; ++i) {
        const int fd = epoll_create1(0);
        if (fd != -1) {
            _epolls.push_back(fd);
        } else {
            perror("epoll_create");
            throw std::logic_error("coudn't epoll_create");
        }
    }

    for (size_t i=0; i<_epolls.size(); ++i) {
        client_worker* wrk = new client_worker(_epolls.at(i));
        wrk->start();
        _workers.push_back(wrk);
    }
}

void client::uninit() noexcept
{
    for (auto worker : _workers) {
        worker->stop();
        delete  worker;
    }
    _workers.clear();

    for (int epoll : _epolls) {
        close(epoll);
    }
    _epolls.clear();
}

void client::send(const request& request, const in_response_handler& handler)
{
    const int sock_d = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_d == -1) {
        perror("socket");
        handler(nullptr);
        return;
    }

    struct addrinfo* addrs;
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    if (getaddrinfo(request.remote_host.data(),
                    std::to_string(request.remote_port).data(),
                    &hints,
                    &addrs) != 0) {
        close(sock_d);
        perror("getaddrinfo");
        handler(nullptr);
        return;
    }

    bool was_connected = false;
    for (struct addrinfo *addr = addrs; addr != nullptr; addr = addr->ai_next) {
        if (connect(sock_d, addr->ai_addr, addr->ai_addrlen) == 0) {
            was_connected = true;
            break;
        } else {
            perror("connect");
        }
    }

    freeaddrinfo(addrs);

    if (!was_connected) {
        close(sock_d);
        handler(nullptr);
        return;
    }

    if (fcntl(sock_d, F_SETFL, O_NONBLOCK) != 0) {
        close(sock_d);
        perror("fcntl to NONBLOCK");
        handler(nullptr);
        return;
    }

    connection* conn = new connection;
    conn->sock_d = sock_d;
    conn->in_resp_handler = handler;
    conn->state = connection_state::write_request;
    try {
        conn->req_reader = std::make_unique<request_reader>(request);
    } catch (const std::exception& e) {
        LOG(ERROR) << "couldn't alloc request_reader: " << e.what();
        close(sock_d);
        delete conn;
        handler(nullptr);
        return;
    }

    // setup epoll
    // TODO: refactoring: move managing of epoll to worker and
    // create and use worker::add_connection(connection* conn)
    epoll_event in_event;
    in_event.events = EPOLLOUT | EPOLLRDHUP;
    in_event.data.ptr = conn;
    if (epoll_ctl(_epolls.at(_current_epoll_index), EPOLL_CTL_ADD, sock_d, &in_event) == -1) {
        close(sock_d);
        delete conn;
        perror("epoll_ctl");
        handler(nullptr);
        return;
    }

    ++_current_epoll_index;
    if (_current_epoll_index >= _epolls.size()) {
        _current_epoll_index = 0;
    }
}
