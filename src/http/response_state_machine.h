#ifndef RESPONSE_STATE_MACHINE_H
#define RESPONSE_STATE_MACHINE_H

#include <tuple>
#include <memory>

#include "str.h"
#include "response.h"

namespace http {

class response_state_machine
{
public:
    enum class state
    {
        processing,
        accpeted,
        rejected
    };

    response_state_machine();
    ~response_state_machine();

    state get_state() const noexcept;
    int get_rejected_code() const noexcept;
    std::shared_ptr<response> get_response() const noexcept;

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
        read_version,
        read_status,
        read_status_description,
        read_headers,
        read_body,
        read_none
    };

private:
    inline std::tuple<bool, int>
    handle_read_version(const char* buff, size_t size) noexcept;

    inline std::tuple<bool, int>
    handle_read_status(const char* buff, size_t size) noexcept;

    inline std::tuple<bool, int>
    handle_read_status_description(const char* buff, size_t size) noexcept;

    inline std::tuple<bool, int>
    handle_read_headers(const char* buff, size_t size) noexcept;

    void go_next(char* buff, size_t size) noexcept;
    void go_final_error(int code) noexcept;
    void go_final_success() noexcept;

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
    read_state _read_state = read_state::read_version;

    int _rejected_code = 0;
    std::shared_ptr<response> _response;
};

}

#endif // RESPONSE_STATE_MACHINE_H
