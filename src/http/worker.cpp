#include "worker.h"

#include <cassert>

#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <glog/logging.h>

#include "connection.h"

using namespace http;

// TODO: 408 Request Timeout

worker::worker(int epoll_d) noexcept :
    _epoll_d(epoll_d)
{
    _isRuning.store(false);
}

worker::~worker()
{
    stop();
}

void worker::start() noexcept
{
    LOG(INFO) << "Listen " << _epoll_d;
    _isRuning.store(true);
    _thread = std::thread(&worker::loop, this);
}

void worker::stop() noexcept
{
    _isRuning.store(true);
    if (_thread.joinable()) {
        _thread.join();
    }
}

void worker::loop() noexcept
{
    const int timeout_msecs = 30000;
    const size_t max_events = 1000;
    epoll_event events[max_events];

    while (_isRuning) {
        int num_conns = epoll_wait(_epoll_d, events, max_events, timeout_msecs);
        for (int i = 0; i < num_conns; ++i) {
            const epoll_event& event = events[i];
            connection* conn = reinterpret_cast<connection*>(event.data.ptr);
            if (event.events&EPOLLRDHUP) {
                go_close_connection(conn);
            } else if (event.events&EPOLLIN) {
                handle_in(conn);
                if (conn->state == connection_state::write_response) {
                    epoll_event event;
                    event.events = EPOLLOUT | EPOLLRDHUP;
                    event.data.ptr = conn;
                    if (epoll_ctl(_epoll_d, EPOLL_CTL_MOD, conn->sock_d, &event) == -1) {
                        perror("epoll_ctl mod");
                        go_close_connection(conn);
                    }
                }
            } else if (event.events&EPOLLOUT) {
                handle_out(conn);
            } else {
                assert(false);
            }
        }
    }
}

void worker::handle_in(connection *conn) noexcept
{
    if (conn->state != connection_state::read_request) {
        LOG(WARNING) << "try handle in but it's incorect state";
    }

    auto&& req_state_machine = conn->req_state_machine;

    while (req_state_machine->get_state() == request_state_machine::state::processing) {
        auto [buff, size] = req_state_machine->prepare_buff();
        if (size > 0) {
            const ssize_t s_read_size = read(conn->sock_d, buff, size);
            if (s_read_size > 0) {
                req_state_machine->process_buff(static_cast<size_t>(s_read_size));
            } else if (s_read_size == -1 && errno == EAGAIN) {
                return;
            } else {
                perror("read request");
                response resp;
                resp.code = 500;
                go_write_response(conn, resp);
                return;
            }
        }
    }

    switch(req_state_machine->get_state()) {
    case request_state_machine::state::processing: {
        break;
    }
    case request_state_machine::state::rejected: {
        response resp;
        resp.code = req_state_machine->get_rejected_code();
        go_write_response(conn, resp);
        break;
    }
    case request_state_machine::state::accpeted: {
        response resp = conn->req_handler(req_state_machine->get_request());
        go_write_response(conn, resp);
        break;
    }
    }
}

void worker::handle_out(connection *conn) noexcept
{
    if (conn->state != connection_state::write_response) {
        LOG(WARNING) << "try handle out but it's incorect state";
    }

    auto&& resp_reader = conn->resp_reader;

    while (resp_reader->has_chunks()) {
        response_chunk chunk = resp_reader->get_chunk();

        ssize_t written = -1;
        if (chunk.buff != nullptr) {
            written = write(conn->sock_d, chunk.buff, chunk.size);
        } else if (chunk.file_d != -1) {
            written = sendfile(conn->sock_d, chunk.file_d, &chunk.file_offset, chunk.size);
        } else {
            assert(1);
        }

        if (written > 0) {
            resp_reader->next(static_cast<size_t>(written));
        } else if (written == -1 && errno == EAGAIN) {
            break;
        } else {
            perror("write response");
            go_close_connection(conn);
            break;
        }
    }

    if (!resp_reader->has_chunks()) {
        go_close_connection(conn);
    }
}

void worker::go_write_response(connection *conn, const response& resp) noexcept
{
    conn->resp_reader = std::make_unique<response_reader>(resp);
    conn->state = connection_state::write_response;
}

void worker::go_close_connection(connection *conn) noexcept
{
    close(conn->sock_d);
    delete conn;
}
