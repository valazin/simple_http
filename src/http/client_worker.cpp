#include "client_worker.h"

#include <cassert>

#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <glog/logging.h>

#include "connection.h"

using namespace http;

client_worker::client_worker(int epoll_d) noexcept :
    _epoll_d(epoll_d)
{
    _isRuning.store(false);
}

client_worker::~client_worker()
{
    stop();
}

void client_worker::start() noexcept
{
    LOG(INFO) << "Listen " << _epoll_d;
    _isRuning.store(true);
    _thread = std::thread(&client_worker::loop, this);
}

void client_worker::stop() noexcept
{
    _isRuning.store(true);
    if (_thread.joinable()) {
        _thread.join();
    }
}

void client_worker::loop() noexcept
{
    const int timeout_msecs = 30000;
    const size_t max_events = 1000;
    epoll_event events[max_events];

    while (_isRuning) {
        int num_conns = epoll_wait(_epoll_d, events, max_events, timeout_msecs);
        for (int i = 0; i < num_conns; ++i) {
            const epoll_event& event = events[i];
            connection* conn = reinterpret_cast<connection*>(event.data.ptr);
            if (event.events&EPOLLIN) {
                handle_in(conn);
            } else if (event.events&EPOLLOUT) {
                handle_out(conn);
                if (conn->state == connection_state::read_response) {
                    epoll_event event;
                    event.events = EPOLLIN | EPOLLRDHUP;
                    event.data.ptr = conn;
                    if (epoll_ctl(_epoll_d, EPOLL_CTL_MOD, conn->sock_d, &event) == -1) {
                        perror("epoll_ctl mod");
                        go_close_connection(conn);
                    }
                }
            } else if (event.events&EPOLLRDHUP) {
                go_close_connection(conn);
            } else {
                assert(false);
            }
        }
    }
}

void client_worker::handle_in(connection *conn) noexcept
{
    if (conn->state != connection_state::read_response) {
        LOG(WARNING) << "try handle in but it's incorect state";
    }

    auto&& resp_state_machine = conn->resp_state_machine;

    while (resp_state_machine->get_state() == response_state_machine::state::processing) {
        auto [buff, size] = resp_state_machine->prepare_buff();
        if (size > 0) {
            const ssize_t s_read_size = read(conn->sock_d, buff, size);
            if (s_read_size > 0) {
                resp_state_machine->process_buff(static_cast<size_t>(s_read_size));
            } else if (s_read_size == -1 && errno == EAGAIN) {
                return;
            } else {
                // TODO: error enum
                go_close_connection_by_error(conn, 501);
                return;
            }
        }
    }

    switch(resp_state_machine->get_state()) {
    case response_state_machine::state::processing: {
        break;
    }
    case response_state_machine::state::rejected: {
        // TODO: error enum
        go_close_connection_by_error(conn, 500 + resp_state_machine->get_rejected_code());
        break;
    }
    case response_state_machine::state::accpeted: {
        conn->resp_handler(resp_state_machine->get_response());
        go_close_connection(conn);
        break;
    }
    }
}

void client_worker::handle_out(connection *conn) noexcept
{
    if (conn->state != connection_state::write_request) {
        LOG(WARNING) << "try handle out but it's incorect state";
    }

    auto&& req_reader = conn->req_reader;

    while (req_reader->has_chunks()) {
        request_chunk chunk = req_reader->get_chunk();

        ssize_t written = -1;
        if (chunk.buff != nullptr) {
            written = write(conn->sock_d, chunk.buff, chunk.size);
        } else if (chunk.file_d != -1) {
            written = sendfile(conn->sock_d, chunk.file_d, &chunk.file_offset, chunk.size);
        } else {
            assert(1);
        }

        if (written > 0) {
            req_reader->next(static_cast<size_t>(written));
        } else if (written == -1 && errno == EAGAIN) {
            break;
        } else {
            perror("write request");
            // TODO: enum error
            go_close_connection_by_error(conn, 601);
            return;
        }
    }

    if (!req_reader->has_chunks()) {
        go_read_response(conn);
    }
}

void client_worker::go_read_response(connection *conn) noexcept
{
    conn->resp_state_machine = std::make_unique<response_state_machine>();
    conn->state = connection_state::read_response;
}

void client_worker::go_close_connection(connection *conn) noexcept
{
    close(conn->sock_d);
    delete conn;
}

void client_worker::go_close_connection_by_error(connection *conn, int err_code) noexcept
{
    LOG(ERROR) << "close with error " << err_code;

    auto resp = std::make_shared<response>();
    resp->code = err_code;

    conn->resp_handler(resp);

    go_close_connection(conn);
}
