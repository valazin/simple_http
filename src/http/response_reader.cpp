#include "response_reader.h"

#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include "../utility/filesystem.h"

http::response_reader::response_reader(const response &resp) :
    _resp(resp)
{
    if (!_resp.body_file_path.empty()) {
        _body_fd = open(_resp.body_file_path.data(), O_RDONLY);
        if (_body_fd == -1) {
            // TODO:
//            perror("open file to write body");
//            release_connection(conn);
//            return;
        }

        ssize_t size = filesystem::file_size(_body_fd);
        if (size != -1) {
            _body_size = static_cast<size_t>(size);
        } else {
            // TODO:
//            perror("get file size");
//            release_connection(conn);
//            return;

        }
    } else if (!_resp.body_str.empty()) {
        _body_size = _resp.body_str.size();
    } else if (_resp.body_buff != nullptr) {
        _body_size = _resp.body_buff->size();
    }

    std::stringstream ss;
    ss << "HTTP/1.1 " << _resp.code << " " << status_code_to_str(_resp.code) << "\r\n";
    ss << "Access-Control-Allow-Origin: *" << "\r\n";
    if (_body_size > 0) {
        ss << "Content-Length: " << _body_size << "\r\n";
        switch(_resp.content_type) {
        case content_types::none:
            break;
        case content_types::text:
            ss << "Content-Type: text/plain" << "\r\n";
            break;
        case content_types::json:
            ss << "Content-Type: application/json" << "\r\n";
            break;
        case content_types::hls_chunk:
            ss << "Content-Type: video/MP2T" << "\r\n";
            break;
        case content_types::hls_playlist:
            ss << "Content-Type: application/vnd.apple.mpegurl" << "\r\n";
            break;
        }
    }
    ss << "\r\n";

    _line = ss.str();
}

http::response_reader::~response_reader()
{
    if (_body_fd != -1) {
        close(_body_fd);
    }
}

int http::response_reader::resp_code() const noexcept
{
    return _resp.code;
}

bool http::response_reader::has_chunks() const noexcept
{
    return _state != state::read_none;
}

http::response_chunk http::response_reader::get_chunk() const noexcept
{
    response_chunk res;

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
        res.buff = _resp.body_str.data() + _body_written_size;
        res.size = _body_size - _body_written_size;
        break;
    case state::read_body_buff:
        res.buff = _resp.body_buff->data() + _body_written_size;
        res.size = _body_size - _body_written_size;
        break;
    case state::read_none:
        break;
    }

    return res;
}

void http::response_reader::next(size_t size) noexcept
{
    switch(_state) {
    case state::read_line:
        _line_written_size += size;
        if (_line_written_size >= _line.size()) {
            if (!_resp.body_file_path.empty()) {
                _state = state::read_body_file;
            } else if (!_resp.body_str.empty()) {
                _state = state::read_body_str;
            } else if (_resp.body_buff != nullptr) {
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

// TODO: move to http::statues enum
std::string http::response_reader::status_code_to_str(int code) noexcept
{
    switch (code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not found";
    case 411:
        return "Length Required";
    case 413:
        return "Payload Too Large";
    case 500:
        return "Internal Error";
    }
    return "Unknow";
}
