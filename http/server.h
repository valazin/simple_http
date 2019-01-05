#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

namespace http
{

class request;
class worker;

class server
{
public:
    server();
    ~server();

    bool start(const std::string& host,
               uint16_t port,
               std::function<void(request* request)> handler);
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

    std::function<void(request* request)> _handler;
};

}

#endif // SERVER_H
