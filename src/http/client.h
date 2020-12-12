#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <atomic>

#include "request.h"
#include "handlers.h"

namespace http {

struct connection;
class client_worker;

class client
{
public:
    client();
    ~client();

    bool send(const request& request, const in_response_handler& handler);

private:
    void init();
    void uninit() noexcept;

private:
    std::atomic<size_t> _current_epoll_index = 0;
    std::vector<int> _epolls;
    std::vector<client_worker*> _workers;
};

}

#endif // CLIENT_H
