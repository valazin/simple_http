#ifndef REQUEST_STATE_MACHINE_H
#define REQUEST_STATE_MACHINE_H

#include <tuple>
#include <functional>

#include "string"
#include "request.h"
#include "handlers.h"

namespace http {

class request_state_machine
{
public:
    enum class state
    {
        processing,
        accpeted,
        rejected
    };

    request_state_machine(uri_handler uri, header_handler header);
    ~request_state_machine();

    state get_state() const noexcept;
    int get_rejected_code() const noexcept;
    std::shared_ptr<request> get_request() const noexcept;

    std::tuple<char*,size_t> prepare_buff() noexcept;
    void process_buff(size_t size) noexcept;

private:
    enum class wait_state
    {
        wait_sp,
        wait_crlf,
        wait_all,
        wait_none
    };

    enum class read_state
    {
        read_method,
        read_uri,
        read_version,
        read_headers,
        read_body,
        read_none
    };

private:
    inline std::tuple<bool, int>
    handle_read_method(const char* buff, size_t size) noexcept;

    inline std::tuple<bool, int>
    handle_read_uri(const char* buff, size_t size) noexcept;

    inline std::tuple<bool, int>
    handle_read_version(const char* buff, size_t size) noexcept;

    inline std::tuple<bool, int>
    handle_read_headers(const char* buff, size_t size) noexcept;

    void go_next(char* buff, size_t size) noexcept;
    void go_final_error(int code) noexcept;
    void go_final_success() noexcept;

    static request_method
    str_to_request_method(const char* str, size_t size) noexcept;

    static std::pair<http::string, http::string>
    parse_header(const char* buff, size_t size) noexcept;

private:
    const size_t max_payload_size = 2*1024;
    const size_t max_content_size = 10*1024*1024;

    char* _buff = nullptr;
    size_t _buff_size = 0;
    size_t _buff_written_size = 0;
    size_t _buff_processed_size = 0;

    size_t _content_length = 0;

    bool _got_sp = false;
    bool _got_cr = false;
    bool _got_lf = false;
    bool _need_transit = false;
    state _state = state::processing;
    wait_state _wait_state = wait_state::wait_sp;
    read_state _read_state = read_state::read_method;

    int _rejected_code = 0;
    std::shared_ptr<request> _request;

    uri_handler _uri_handler = nullptr;
    header_handler _header_handler = nullptr;
};

}

#endif // REQUEST_STATE_MACHINE_H
