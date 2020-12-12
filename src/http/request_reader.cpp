#include "request_reader.h"

#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include "../utility/filesystem.h"

using namespace http;

request_reader::request_reader(const request& request) :
    _req(request)
{
    if (!_req.body_file_path.empty()) {
        _body_fd = open(_req.body_file_path.data(), O_RDONLY);
        if (_body_fd == -1) {
            perror("couldn't open file");
            throw std::logic_error("coudn't open file " + _req.body_file_path);
        }

        const ssize_t size = filesystem::file_size(_body_fd);
        if (size != -1) {
            _body_size = static_cast<size_t>(size);
        } else {
            close(_body_fd);
            perror("coudn't get size for file");
            throw std::logic_error("coudn't get size of file " + _req.body_file_path);
        }
    } else if (!request.body_str.empty()) {
        _body_size = _req.body_str.size();
    } else if (_req.body_buff != nullptr) {
        _body_size = _req.body_buff->size();
    }

    std::stringstream ss;
    ss << request_method_to_str(_req.method) << " " << _req.uri << " HTTP/1.1\r\n";
    for (const auto& [key, value] : _req.headers) {
        ss << key << ": " << value << "\r\n";
    }
    ss << "Host: " << _req.remote_host << ":" << _req.remote_port << "\r\n";
    if (_body_size > 0) {
        ss << "Content-Length: " << _body_size << "\r\n";
        // TODO: bug: write content type
    }
    ss << "\r\n";

    _line = ss.str();
}

request_reader::~request_reader()
{
    if (_body_fd != -1) {
        close(_body_fd);
    }
}

const request &request_reader::get_request() const
{
    return _req;
}

bool request_reader::has_chunks() const noexcept
{
    return _state != state::read_none;
}

request_reader::chunk request_reader::get_chunk() const noexcept
{
    chunk res;

    switch(_state) {
    case state::read_line:
        if (_line.size() > _line_written_size) {
            res.buff = _line.data() + _line_written_size;
            res.size = _line.size() - _line_written_size;
        }
        break;
    case state::read_body_file:
        res.file_d = _body_fd;
        res.file_offset = static_cast<off_t>(_body_written_size);
        res.size = _body_size - _body_written_size;
        break;
    case state::read_body_str:
        res.buff = _req.body_str.data() + _body_written_size;
        res.size = _body_size - _body_written_size;
        break;
    case state::read_body_buff:
        res.buff = _req.body_buff->data() + _body_written_size;
        res.size = _body_size - _body_written_size;
        break;
    case state::read_none:
        break;
    }

    return res;
}

void request_reader::next(size_t size) noexcept
{
    switch(_state) {
    case state::read_line:
        _line_written_size += size;
        if (_line_written_size >= _line.size()) {
            if (!_req.body_file_path.empty()) {
                _state = state::read_body_file;
            } else if (!_req.body_str.empty()) {
                _state = state::read_body_str;
            } else if (_req.body_buff != nullptr) {
                _state = state::read_body_buff;
            } else {
                _state = state::read_none;
            }
        }
        break;
    case state::read_body_file:
    case state::read_body_str:
    case state::read_body_buff:
        _body_written_size += size;
        if (_body_written_size >= _body_size) {
            _state = state::read_none;
        }
        break;
    case state::read_none:
        break;
    }
}

// TODO: refactoring: move to utils
std::string request_reader::request_method_to_str(request_method method) noexcept
{
    switch (method) {
    case request_method::get:
        return "GET";
    case request_method::post:
        return "POST";
    case request_method::options:
        return "OPTIONS";
    case request_method::undefined:
        return "";
    }
    return "";
}
