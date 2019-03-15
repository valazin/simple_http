#ifndef WORKER_H
#define WORKER_H

#include <string>
#include <thread>
#include <atomic>

#include "response.h"

namespace http {

struct connection;

class worker
{
public:
    worker(int epoll_d) noexcept;
    ~worker();

    void start() noexcept;
    void stop() noexcept;

private:
    void loop() noexcept;

    void handle_in(connection* conn) noexcept;
    void handle_out(connection* conn) noexcept;

    void go_write_response(connection* conn, const response &resp) noexcept;
    void go_close_connection(connection* conn) noexcept;

private:
    int _epoll_d = -1;
    std::atomic<bool> _isRuning;
    std::thread _thread;
};

}

#endif // WORKER_H
