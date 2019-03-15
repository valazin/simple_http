#ifndef REQUEST_H
#define REQUEST_H

#include <map>
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
    request_method method = request_method::undefined;
    std::string uri;
    std::map<std::string,std::string> headers;

    //
    std::string body_str;
    //
    std::shared_ptr<buffer> body_buff;
    //
    std::string body_file_path;

    std::shared_ptr<void> user_data;
};

}

#endif // REQUEST_H
