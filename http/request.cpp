#include "request.h"

#include <iostream>
#include <cstring>

using namespace http;

std::string request_helper::status_code_to_str(int code)
{
    switch (code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 500:
        return "Internal Error";
    }
    return "Unknow";
}

request_line_method request_helper::str_to_request_method(const char* str, size_t size)
{
    request_line_method result = request_line_method::none;
    if (size == 4) {
        if (strncasecmp(str, "post", size) == 0) {
            result = request_line_method::post;
        }
    } else if (size == 3) {
        if (strncasecmp(str, "get", size) == 0) {
            result = request_line_method::get;
        }
    }
    return result;
}

bool request_helper::request_buff_append(request* req, const char* buff, size_t size)
{
    if (req->buff == nullptr) {
        req->buff = reinterpret_cast<char*>(malloc(size));
        if (!req->buff) {
            std::cout << "couldn't allocate memory";
            return false;
        }
        memcpy(req->buff, buff, size);
        req->buf_size = size;
    } else {
        char* new_buff = reinterpret_cast<char*>(realloc(req->buff, req->buf_size + size));
        if (!new_buff) {
            std::cout << "couldn't allocate memory";
            return false;
        }
        memcpy(new_buff+req->buf_size, buff, size);
        req->buff = new_buff;
        req->buf_size += size;
    }
    return true;
}
