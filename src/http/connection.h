#ifndef CONNECTION_H
#define CONNECTION_H

#include <memory>

#include "request.h"
#include "response.h"
#include "response_reader.h"
#include "request_reader.h"
#include "request_state_machine.h"
#include "response_state_machine.h"

namespace http {

enum class connection_state
{
    read_request,
    write_response,
    read_response,
    write_request
};

struct connection
{
    int sock_d = 0;
    
    uint16_t remote_port = 0;
    std::string remote_host;

    connection_state state = connection_state::read_request;

    std::unique_ptr<response_reader> resp_reader;
    std::unique_ptr<request_state_machine> req_state_machine;
    in_request_handler in_req_handler = nullptr;

    std::unique_ptr<request_reader> req_reader;
    std::unique_ptr<response_state_machine> resp_state_machine;
    in_response_handler in_resp_handler = nullptr;
};

}

#endif // CONNECTION_H
