#include "request.h"

#include <cstring>

using namespace http;

std::string request_helper::status_code_to_str(int code)
{
    switch (code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not found";
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
        if (!(*buff)) {
            return false;
        }

        memcpy(req->buff, buff, size);
        req->buff_size = size;
    } else {
        char* new_buff = reinterpret_cast<char*>(realloc(req->buff, req->buff_size + size));
        if (!new_buff) {
            return false;
        }

        memcpy(new_buff+req->buff_size, buff, size);

        req->buff = new_buff;
        req->buff_size += size;
    }
    return true;
}

bool request_helper::request_body_buff_append(request *req, const char *buff, size_t size)
{
    if (req->body.buff == nullptr) {
        req->body.buff = reinterpret_cast<char*>(malloc(req->body.wait_size));
        if (!(*buff)) {
            return false;
        }
    }

   size_t lost = req->body.wait_size - req->body.buff_size;
   if (size > lost) {
       size = lost;
   }

    if (size > 0) {
        memcpy(req->body.buff+req->body.buff_size, buff, size);
        req->body.buff_size += size;
        return true;
    } else {
        return false;
    }
}
