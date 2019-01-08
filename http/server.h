#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

#include "string.h"
#include "request.h"

namespace http
{

class worker;

class server
{
public:
    server();
    ~server();

    bool start(const std::string& host,
               uint16_t port,
               std::function<void(request*)> request_handler,
               std::function<handle_res(request*, http::string)> uri_handler,
               std::function<handle_res(request*, http::string, http::string)> header_handler);
    void stop();

private:
    bool init(const std::string& host, uint16_t port);
    void uninit();
    void loop();

private:
    int _sd = -1;
    std::vector<int> _epolls;
    std::vector<worker*> _workers;

    std::atomic<bool> _isRunning;
    std::thread _thread;

    std::function<void(request* request)> _request_handler;
    std::function<handle_res(request*, http::string)> _uri_handler;
    std::function<handle_res(request*, http::string, http::string)> _header_handler;
};

}

#endif // SERVER_H
