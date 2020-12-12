#include "request_state_machine.h"

#include <cassert>
#include <cstring>

using namespace http;

// TODO: refactoring: create a rejected char list for every state and use it while parsing

request_state_machine::request_state_machine(uri_handler uri,
                                             header_handler header) :
    _uri_handler(uri),
    _header_handler(header)
{
    _request = std::make_shared<request>();

    _buff = new char[max_payload_size];
    _buff_size = max_payload_size;
}

request_state_machine::~request_state_machine()
{
    if (_buff != nullptr) {
        delete[] _buff;
    }
}

request_state_machine::state
request_state_machine::get_state() const noexcept
{
    return _state;
}

int request_state_machine::get_rejected_code() const noexcept
{
    if (_state == state::rejected) {
        return _rejected_code;
    } else {
        return 0;
    }
}

std::shared_ptr<request>
request_state_machine::get_request() const noexcept
{
    if (_state == state::accpeted) {
        return _request;
    } else {
        return nullptr;
    }
}

std::tuple<char *, size_t>
request_state_machine::prepare_buff() noexcept
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

void request_state_machine::process_buff(size_t size) noexcept
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
request_state_machine::handle_read_method(const char *buff, size_t size) noexcept
{
    _request->method = str_to_request_method(buff, size);
    if (_request->method != request_method::undefined) {
        return {true, 0};
    } else {
        return {false, 400};
    }
}

std::tuple<bool, int>
request_state_machine::handle_read_uri(const char *buff, size_t size) noexcept
{
    uri u(buff,size);
    if (u.is_valid()) {
        int res = -1;
        if (_uri_handler) {
            res = _uri_handler(_request, u);
        }

        if (res == -1) {
            _request->uri = u.to_str();
            return {true, 0};
        } else if (res == 0) {
            return {true, 0};
        } else {
            return {false, res};
        }

    } else {
        return {false, 400};
    }
}

std::tuple<bool, int>
request_state_machine::handle_read_version(const char *buff, size_t size) noexcept
{
    string version(buff, size);
    if (version.compare("HTTP/1.1") == 0) {
        return {true, 0};
    } else {
        return {false, 400};
    }
}

std::tuple<bool, int>
request_state_machine::handle_read_headers(const char *buff, size_t size) noexcept
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
            int res = -1;
            if (_header_handler) {
                res = _header_handler(_request, key, value);
            }

            if (res == -1) {
                _request->headers.insert({std::string(key.data(), key.size()),
                                          std::string(value.data(), value.size())});
                return {true, 0};
            } else if (res == 0) {
                return {true, 0};
            } else {
                return {false, res};
            }
        } else {
            return {false, 400};
        }
    }
}

void request_state_machine::go_next(char *buff, size_t size) noexcept
{
    switch (_read_state) {
    case read_state::read_method: {
        auto [ok, code] = handle_read_method(buff, size);
        if (ok) {
            _read_state = read_state::read_uri;
            _wait_state = wait_state::wait_sp;
        } else {
            go_final_error(code);
        }
        break;
    }

    case read_state::read_uri: {
        auto [ok, code] = handle_read_uri(buff, size);
        if (ok) {
            _read_state = read_state::read_version;
            _wait_state = wait_state::wait_crlf;
        } else {
            go_final_error(code);
        }
        break;
    }

    case read_state::read_version: {
        auto [ok, code] = handle_read_version(buff, size);
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
            switch (_request->method) {
            case request_method::post: {
                if (_content_length <= 0) {
                    go_final_error(411);
                    return;
                }

                if (_content_length > max_content_size) {
                    go_final_error(413);
                    return;
                }

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

                break;
            }
            case request_method::options:
            case request_method::get:
                if (_content_length == 0) {
                    go_final_success();
                } else {
                    go_final_error(400);
                }
                break;
            case request_method::undefined:
                assert(false);
                break;
            }
        }
        break;
    }

    case read_state::read_body: {
        if (_buff_written_size >= _buff_size) {
            _request->body_buff = std::make_shared<buffer>(_buff, _buff_written_size);

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

void request_state_machine::go_final_error(int code) noexcept
{
    _rejected_code = code;

    _state = state::rejected;
    _wait_state = wait_state::wait_none;
    _read_state = read_state::read_none;
}

void request_state_machine::go_final_success() noexcept
{
    _rejected_code = 0;

    _state = state::accpeted;
    _wait_state = wait_state::wait_none;
    _read_state = read_state::read_none;
}

request_method
request_state_machine::str_to_request_method(const char *str,
                                             size_t size) noexcept
{
    request_method result = request_method::undefined;
    if (size == 4 && strncmp(str, "POST", size) == 0) {
        result = request_method::post;
    } else if (size == 3 && strncmp(str, "GET", size) == 0) {
        result = request_method::get;
    } else if (size == 7 && strncmp(str, "OPTIONS", size) == 0) {
        result = request_method::options;
    }
    return result;
}

std::pair<string, string>
request_state_machine::parse_header(const char *buff, size_t size) noexcept
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
