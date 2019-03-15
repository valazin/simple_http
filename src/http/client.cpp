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

#include "connection.h"
#include "client_worker.h"

using namespace http;

client::client() noexcept
{
    const size_t epoll_num = 1;
    _epolls.reserve(epoll_num);
    for (size_t i=0; i<epoll_num; ++i) {
        int fd = epoll_create1(0);
        if (fd != -1) {
            _epolls.push_back(fd);
        } else {
            // TODO:
            perror("epoll_create");
//            return false;
        }
    }

    for (size_t i=0; i<_epolls.size(); ++i) {
        client_worker* wrk = new client_worker(_epolls.at(i));
        wrk->start();
        _workers.push_back(wrk);
    }
}

client::~client()
{
    // TODO: check release resources and memory
}

bool client::send(const request &request,
                  const std::string &host,
                  uint16_t port,
                  const std::string &uri,
                  const response_handler &handler)
{
    struct addrinfo* addrs;

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    int err = getaddrinfo(host.data(), std::to_string(port).data(), &hints, &addrs);
    if (err != 0) {
        perror("getaddrinfo");
        return false;
    }

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("socket");
        return false;
    }

    bool was_connected = false;
    for (struct addrinfo *addr = addrs; addr != nullptr; addr = addr->ai_next) {
        if (connect(sd, addr->ai_addr, addr->ai_addrlen) == 0) {
            was_connected = true;
            break;
        } else {
            perror("connect");
        }
    }

    freeaddrinfo(addrs);

    if (!was_connected) {
        close(sd);
        return false;
    }

    if (fcntl(sd, F_SETFL, O_NONBLOCK) != 0) {
        perror("fcntl to NONBLOCK");
        return false;
    }

    connection* conn = new connection;
    conn->sock_d = sd;
    conn->resp_handler = handler;
    conn->state = connection_state::write_request;
    conn->req_reader = std::make_unique<request_reader>(request, host, port, uri);

    epoll_event in_event;
    in_event.events = EPOLLOUT | EPOLLRDHUP;
    in_event.data.ptr = conn;

    if (epoll_ctl(_epolls.at(_current_epoll_index), EPOLL_CTL_ADD, sd, &in_event) == -1) {
        perror("epoll_ctl");
    }

    ++_current_epoll_index;
    if (_current_epoll_index >= _epolls.size()) {
        _current_epoll_index = 0;
    }

    return true;
}
