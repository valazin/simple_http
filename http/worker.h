#ifndef WORKER_H
#define WORKER_H

#include <string>
#include <thread>
#include <atomic>

#include <sys/epoll.h>

#include "string.h"

namespace http
{

struct request;

class worker
{
public:
    worker(int epoll_d) noexcept;
    ~worker();

    void start() noexcept;
    void stop() noexcept;

private:
    void release_request(request* req) noexcept;
    ssize_t write_response_body(request* req) noexcept;

    void go_final_success(request* req) noexcept;
    void go_final_error(request* req, int code, const std::string& error) noexcept;
    void go_next(request* req, const char* buff, size_t size) noexcept;

    void handle_in(request* req) noexcept;
    void handle_out(request* req) noexcept;
    void loop() noexcept;

    static std::pair<http::string, http::string> parse_header(const char* buff, size_t size) noexcept;

private:
    int _epoll_d = -1;
    std::atomic<bool> _isRuning;
    std::thread _thread;

    static const int timeout_msecs = 30000;
    static const size_t max_events = 10000;
    epoll_event events[max_events];

    static const size_t in_buff_size = 2*1000000;
    char in_buff[in_buff_size];
};

}

#endif // WORKER_H
