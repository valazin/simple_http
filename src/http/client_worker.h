#ifndef CLIENT_WORKER_H
#define CLIENT_WORKER_H

#include <string>
#include <thread>
#include <atomic>

namespace http {

struct connection;
struct response;

class client_worker
{
public:
    client_worker(int epoll_d) noexcept;
    ~client_worker();

    void start() ;
    void stop() noexcept;

private:
    void loop() noexcept;

    void handle_out(connection* conn) noexcept;
    void handle_in(connection* conn) noexcept;

    void go_read_response(connection* conn) noexcept;
    void go_close_connection_with_success(connection* conn, const std::shared_ptr<http::response>& resp) noexcept;
    void go_close_connection_with_error(connection* conn) noexcept;

    static void release_connection(connection* conn) noexcept;

private:
    int _epoll_d = -1;
    std::atomic<bool> _is_runing = false;
    std::thread _thread;
};

}

#endif // CLIENT_WORKER_H
