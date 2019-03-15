#include "request_reader.h"

#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include <glog/logging.h>

#include "../utility/filesystem.h"

using namespace http;

request_reader::request_reader(const request& request,
                               const std::string &host,
                               uint16_t port,
                               const std::string &uri) :
    _req(request)
{
    if (!_req.body_file_path.empty()) {
        _body_fd = open(_req.body_file_path.data(), O_RDONLY);
        if (_body_fd == -1) {
            // TODO:
            LOG(ERROR) << "Couldn't open file: " << _req.body_file_path;
//            perror("open file to write body");
//            release_connection(conn);
//            return;
        }

        ssize_t size = filesystem::file_size(_body_fd);
        if (size != -1) {
            _body_size = static_cast<size_t>(size);
        } else {
            // TODO:
            LOG(ERROR) << "Couldn't detect file size: " << _req.body_file_path;
//            perror("get file size");
//            release_connection(conn);
//            return;

        }
    } else if (!request.body_str.empty()) {
        _body_size = _req.body_str.size();
    } else if (_req.body_buff != nullptr) {
        _body_size = _req.body_buff->size();
    }

    std::stringstream ss;
    ss << request_method_to_str(_req.method) << " " << uri << " HTTP/1.1\r\n";
    ss << "Host: " << host << ":" << port << "\r\n";
    for (auto&& header : _req.headers) {
        ss << header.first << ": " << header.second << "\r\n";
    }
    if (_body_size > 0) {
        ss << "Content-Length: " << _body_size << "\r\n";
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

bool request_reader::has_chunks() const noexcept
{
    return _state != state::read_none;
}

request_chunk request_reader::get_chunk() const noexcept
{
    request_chunk res;

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
