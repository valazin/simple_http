#include "response_state_machine.h"

#include <cstring>
#include <cassert>

using namespace http;

response_state_machine::response_state_machine()
{
    _response = std::make_shared<response>();

    _buff = new char[max_payload_size];
    _buff_size = max_payload_size;
}

response_state_machine::~response_state_machine()
{
    if (_buff != nullptr) {
        delete[] _buff;
    }
}

http::response_state_machine::state
http::response_state_machine::get_state() const noexcept
{
    return _state;
}

int http::response_state_machine::get_rejected_code() const noexcept
{
    if (_state == state::rejected) {
        return _rejected_code;
    } else {
        return 0;
    }
}

std::shared_ptr<http::response>
http::response_state_machine::get_response() const noexcept
{
    if (_state == state::accpeted) {
        return _response;
    } else {
        return nullptr;
    }
}

std::tuple<char *, size_t>
http::response_state_machine::prepare_buff() noexcept
{
    if (_state == state::processing) {
        if (_buff_size > _buff_written_size) {
            size_t avaible_size = _buff_size - _buff_written_size;
            return {_buff + _buff_written_size, avaible_size};
        } else {
            go_final_error(413);
            return {nullptr, 0};
        }
    }
    return {nullptr, 0};
}

void http::response_state_machine::process_buff(size_t size) noexcept
{
    if (_state != state::processing) {
        return;
    }

    // size of a separators like sp, crlf
    size_t ignore_size = 0;

    // start look for the separators from the end
    size_t current_pos = _buff_written_size;

    _buff_written_size += size;

    assert(_buff_size >= _buff_written_size);
    assert(_buff_written_size >= _buff_processed_size);

    for (; current_pos < _buff_written_size; ++current_pos) {
        if (_wait_state == wait_state::wait_sp) {
            if (_buff[current_pos] == 0x20) {
                ignore_size = 1;
                _need_transit = true;
            }
        } else if (_wait_state == wait_state::wait_crlf) {
            if (!_got_cr) {
                if (_buff[current_pos] == 0x0D) {
                    _got_cr = true;
                    continue;
                }
            } else {
                if (_buff[current_pos] == 0x0A) {
                    ignore_size = 2;
                    _need_transit = true;
                } else {
                    _got_cr = false;
                }
            }
        } else if (_wait_state == wait_state::wait_all) {
            char* buff = _buff + _buff_processed_size;
            size_t size = _buff_written_size - _buff_processed_size;

            _buff_processed_size += size;

            go_next(buff, size);

            break;
        } else if (_wait_state == wait_state::wait_none) {
            break;
        }

        if (_need_transit) {
            _got_sp = false;
            _got_cr = false;
            _got_lf = false;
            _need_transit = false;

            char* buff = _buff + _buff_processed_size;
            size_t size = (current_pos + 1) - _buff_processed_size;

            _buff_processed_size += size;

            go_next(buff, size - ignore_size);
        }
    }
}

std::tuple<bool, int>
response_state_machine::handle_read_version(const char *buff, size_t size) noexcept
{
    string version(buff, size);
    if (version.compare("HTTP/1.1") == 0) {
        return {true, 0};
    } else {
        return {false, 400};
    }
}

std::tuple<bool, int>
response_state_machine::handle_read_status(const char *buff, size_t size) noexcept
{
    string status(buff, size);

    auto [code, ok] = status.to_int<int>();
    if (ok) {
        _response->code = code;
        return {true, 0};
    } else {
        return {false, 400};
    }
}

std::tuple<bool, int>
response_state_machine::handle_read_status_description(const char*, size_t) noexcept
{
    // TODO: feature: add description to response
    return {true, 0};
}

std::tuple<bool, int>
response_state_machine::handle_read_headers(const char *buff, size_t size) noexcept
{
    auto [key, value] = parse_header(buff, size);
    if (key.compare("Content-Length") == 0) {
        bool ok = false;
        int64_t length = value.to_int(ok);
        if (ok && length>=0) {
            _content_length = static_cast<size_t>(length);
            return {true, 0};
        } else {
            return {false, 400};
        }
    } else {
        if (!key.empty()) {
            _response->headers.insert({std::string(key.data(), key.size()), std::string(value.data(), value.size())});
            return {true, 0};
        } else {
            return {false, 400};
        }
    }
}

void response_state_machine::go_next(char *buff, size_t size) noexcept
{
    switch (_read_state) {
    case read_state::read_version: {
        auto [ok, code] = handle_read_version(buff, size);
        if (ok) {
            _read_state = read_state::read_status;
            _wait_state = wait_state::wait_sp;
        } else {
            go_final_error(code);
        }
        break;
    }

    case read_state::read_status: {
        auto [ok, code] = handle_read_status(buff, size);
        if (ok) {
            _read_state = read_state::read_status_description;
            _wait_state = wait_state::wait_crlf;
        } else {
            go_final_error(code);
        }
        break;
    }

    case read_state::read_status_description: {
        auto [ok, code] = handle_read_status_description(buff, size);
        if (ok) {
            _read_state = read_state::read_headers;
            _wait_state = wait_state::wait_crlf;
        } else {
            go_final_error(code);
        }
        break;
    }

    case read_state::read_headers: {
        if (size > 0) {
            auto [ok, code] = handle_read_headers(buff, size);
            if (ok) {
            } else {
                go_final_error(code);
            }
        } else {
            if (_content_length > 0) {
                char* new_buff = new char[_content_length];
                size_t new_buff_size = _content_length;
                size_t new_buff_written_size = 0;

                // save a part of body if it's in req->buff
                if (_buff_written_size > _buff_processed_size) {
                    size_t copy_size = _buff_written_size - _buff_processed_size;
                    if (copy_size > new_buff_size) {
                        copy_size = new_buff_size;
                    }
                    memcpy(new_buff, _buff + _buff_processed_size, copy_size);
                    new_buff_written_size += copy_size;
                }

                delete[] _buff;
                _buff = new_buff;
                _buff_size = new_buff_size;
                _buff_written_size = new_buff_written_size;
                _buff_processed_size = new_buff_written_size;

                _read_state = read_state::read_body;
                _wait_state = wait_state::wait_all;

                // after the prev step we can already have written body
                if (_buff_written_size >= _buff_size) {
                    go_next(nullptr, 0);
                }
            } else {
                go_final_success();
            }
        }
        break;
    }

    case read_state::read_body: {
        if (_buff_written_size >= _buff_size) {
            _response->body_buff = std::make_shared<buffer>(_buff, _buff_written_size);

            _buff = nullptr;
            _buff_size = 0;
            _buff_written_size = 0;
            _buff_processed_size = 0;

            go_final_success();
        } else {
        }
        break;
    }

    case read_state::read_none: {
        break;
    }

    }
}

void response_state_machine::go_final_error(int code) noexcept
{
    _rejected_code = code;

    _state = state::rejected;
    _wait_state = wait_state::wait_none;
    _read_state = read_state::read_none;
}

void response_state_machine::go_final_success() noexcept
{
    _rejected_code = 0;

    _state = state::accpeted;
    _wait_state = wait_state::wait_none;
    _read_state = read_state::read_none;
}

std::pair<string, string>
response_state_machine::parse_header(const char *buff, size_t size) noexcept
{
    http::string header(buff, size);
    http::string key = header.cut_by(':');

    key.trim();
    header.trim();

    std::pair<http::string, http::string> res;
    res.first = key;
    res.second = header;

    return res;
}
