#ifndef CLIENT_WORKER_H
#define CLIENT_WORKER_H

#include <string>
#include <thread>
#include <atomic>

namespace http {

struct connection;

class client_worker
{
public:
    client_worker(int epoll_d) noexcept;
    ~client_worker();

    void start() noexcept;
    void stop() noexcept;

private:
    void loop() noexcept;

    void handle_in(connection* conn) noexcept;
    void handle_out(connection* conn) noexcept;

    void go_read_response(connection* conn) noexcept;
    void go_close_connection(connection* conn) noexcept;
    void go_close_connection_by_error(connection* conn, int err_code) noexcept;

private:
    int _epoll_d = -1;
    std::atomic<bool> _isRuning;
    std::thread _thread;
};

}

#endif // CLIENT_WORKER_H
