#ifndef REQUEST_READER_H
#define REQUEST_READER_H

#include "request.h"

namespace http {

class request_reader
{
public:
    struct chunk
    {
        const char* buff = nullptr;

        int file_d = -1;
        off_t file_offset = 0;

        size_t size = 0;
    };

    explicit request_reader(const request& request);
    ~request_reader();

    const request& get_request() const;

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
    static std::string request_method_to_str(request_method method) noexcept;

private:
    request _req;

    std::string _line;
    size_t _line_written_size = 0;

    int _body_fd = -1;
    size_t _body_size = 0;
    size_t _body_written_size = 0;

    state _state = state::read_line;
};

}

#endif // REQUEST_READER_H
