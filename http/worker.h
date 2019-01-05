#ifndef WORKER_H
#define WORKER_H

#include <string>
#include <thread>
#include <atomic>

#include <sys/epoll.h>

namespace http
{

struct request;

class worker
{
public:
    worker(int epoll_d);
    ~worker();

    void start();
    void stop();

private:
    void go_final_success(request* req);
    void go_final_error(request* req, int code, const std::string& error);
    void go_next(request* req, const char* buff, size_t size);

    void handle_in(request* req);
    void handle_out(request* req);
    void loop();

    static std::pair<std::string, std::string> parse_header(const char* buff, size_t size);
//    static int buff_to_int(const char* buff, size_t size);

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
