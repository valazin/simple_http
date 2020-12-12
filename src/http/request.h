#ifndef REQUEST_H
#define REQUEST_H

#include <unordered_map>
#include <string>
#include <memory>

#include "buffer.h"

namespace http {

enum class request_method
{
    post,
    get,
    options,
    undefined
};

struct request
{
    uint16_t remote_port = 0;
    std::string remote_host;
    std::string uri;

    request_method method = request_method::undefined;
    
    std::unordered_map<std::string,std::string> headers;

    std::string body_str;
    std::shared_ptr<buffer> body_buff;
    std::string body_file_path;

    std::shared_ptr<void> user_data;
};

}

#endif // REQUEST_H
