#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <atomic>

#include "request.h"
#include "handlers.h"

namespace http {

class client_worker;

class client
{
public:
    client() noexcept;
    ~client();

    bool send(const request& request,
              const std::string& host,
              uint16_t port,
              const std::string& uri,
              const response_handler& handler);

private:
    std::atomic<size_t> _current_epoll_index = 0;
    std::vector<int> _epolls;
    std::vector<client_worker*> _workers;
};

}

#endif // CLIENT_H
