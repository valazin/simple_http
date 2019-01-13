#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include <map>
#include <functional>

#include "uri.h"
#include "string.h"

namespace http
{

struct response
{
    int code = 0;

    std::string line;
    size_t line_write_size = 0;

    char* body = nullptr;
    size_t body_write_size = 0;
    size_t body_size = 0;
    bool free_body = true;
};

enum class request_line_method
{
    none,
    post,
    get
};

struct request_line_uri
{
    std::string str;
};

struct request_line_version
{
    std::string str;
};

enum class request_line_state
{
    read_method,
    read_uri,
    read_version,
};

struct request_line
{
    request_line_uri uri;
    request_line_version version;
    request_line_method method = request_line_method::none;
    request_line_state state = request_line_state::read_method;
};

struct request_headers
{
    std::map<std::string, std::string> map;
};

struct request_body
{
    char* buff = nullptr;
    size_t buff_size = 0;
    size_t wait_size = 0;
};

enum class request_state
{
    read_line,
    read_headers,
    read_body,
    write_response
};

enum class request_wait_state
{
    wait_sp,
    wait_crlf,
    wait_all,
    wait_none
};

enum class handle_res_type
{
    ignore,
    success,
    error
};

struct handle_res
{
    handle_res_type type = handle_res_type::ignore;
    // use if error
    int code = 0;
};

struct request
{
    int sock_d = 0;

    char* buff = nullptr;
    size_t buff_size = 0;

    bool got_sp = false;
    bool got_cr = false;
    bool got_lf = false;
    bool need_process = false;

    request_line line;
    request_headers headers;
    request_body body;

    request_state state = request_state::read_line;
    request_wait_state wait_state = request_wait_state::wait_sp;

    response resp;
    void* user_data = nullptr;

    // TODO: test function perfmonce with function pointer
    std::function<void(request*)> request_handler = nullptr;
    std::function<handle_res(request*, uri)> uri_handler = nullptr;
    std::function<handle_res(request*, string, string)> header_handler = nullptr;
};

struct request_helper
{
    static std::string status_code_to_str(int code);
    static request_line_method str_to_request_method(const char* str, size_t size);
    static bool request_buff_append(request* req, const char* buff, size_t size);
    static bool request_body_buff_append(request* req, const char* buff, size_t size);
};

}

#endif // REQUEST_H
