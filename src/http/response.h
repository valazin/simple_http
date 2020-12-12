#ifndef RESPONSE_H
#define RESPONSE_H

#include <unordered_map>
#include <string>
#include <memory>
#include <functional>

#include "buffer.h"
#include "content_types.h"

namespace http {

struct response
{
    response() = default;

    explicit response(int c) :
        code(c)
    {
    }

    int code = 0;

    std::unordered_map<std::string,std::string> headers;

    content_types content_type = content_types::none;
    std::string body_str;
    std::shared_ptr<buffer> body_buff;
    std::string body_file_path;

    // TODO: refactoring: used by server when reponse is finished
    std::function<void (bool res)> callback = nullptr;
};

}

#endif // RESPONSE_H
