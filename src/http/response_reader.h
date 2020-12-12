#ifndef RESPONSE_READER_H
#define RESPONSE_READER_H

#include "response.h"

namespace http {

class response_reader
{
public:
    struct chunk
    {
        const char* buff = nullptr;

        int file_d = -1;
        off_t file_offset = 0;

        size_t size = 0;
    };

    // Response with 500 status code. Use if another construtor thrown an exception
    explicit response_reader() noexcept;
    explicit response_reader(const response& resp);
    ~response_reader();

    const response& get_response() const;

    bool has_chunks() const noexcept;
    chunk get_chunk() const noexcept;
    void next(size_t size) noexcept;

private:
    enum class state
    {
        read_none,
        read_line,
        read_body_str,
        read_body_buff,
        read_body_file
    };

private:
    inline static std::string status_code_to_str(int code) noexcept;

private:
    const response _resp = response(500);

    std::string _line = "HTTP/1.1 500 Internal Error \r\n\r\n";
    size_t _line_written_size = 0;

    int _body_fd = -1;
    size_t _body_size = 0;
    size_t _body_written_size = 0;

    state _state = state::read_line;
};

}

#endif // RESPONSE_READER_H
