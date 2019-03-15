#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

#include "str.h"
#include "request.h"
#include "response.h"
#include "connection.h"
#include "handlers.h"

namespace http {

class worker;

class server
{
public:
    server() noexcept;
    ~server();

    bool start(const std::string& host,
               uint16_t port,
               request_handler request_handl,
               uri_handler uri_hand,
               header_handler header_handl) noexcept;
    void stop() noexcept;

private:
    bool init(const std::string& host, uint16_t port) noexcept;
    void uninit() noexcept;
    void loop() noexcept;

private:
    int _sd = -1;
    std::vector<int> _epolls;
    std::vector<worker*> _workers;

    std::atomic<bool> _isRunning;
    std::thread _thread;

    request_handler _request_handler;
    uri_handler _uri_handler;
    header_handler _header_handler;
};

}

#endif // SERVER_H
