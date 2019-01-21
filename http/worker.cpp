#include "worker.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include <cassert>

#include <glog/logging.h>

#include "request.h"
#include "../utility/filesystem.h"

using namespace http;

worker::worker(int epoll_d) noexcept :
    _epoll_d(epoll_d)
{
    _isRuning.store(false);
}

worker::~worker()
{
    _isRuning.store(true);
    if (_thread.joinable()) {
        _thread.join();
    }
}

void worker::start() noexcept
{
    LOG(INFO) << "Listen " << _epoll_d;
    _isRuning.store(true);
    _thread = std::thread(&worker::loop, this);
}

void worker::stop() noexcept
{
    _isRuning.store(false);
}

void worker::release_request(request *req) noexcept
{
    close(req->sock_d);
    if (req->buff != nullptr) {
        delete[] req->buff;
    }
    if (req->body.buff != nullptr) {
        delete[] req->body.buff;
    }
    if (req->resp.free_cstr && req->resp.body_cstr != nullptr) {
        free(req->resp.body_cstr);
    }
    if (req->resp.body_fd != -1) {
        close(req->resp.body_fd);
    }
    delete req;
}

ssize_t worker::write_response_body(request* req) noexcept
{
    response& resp = req->resp;
    size_t lost = (resp.body_size - resp.body_write_size);
    if (lost <= 0) {
        return 0;
    }

    ssize_t written = -1;
    if (resp.body_cstr != nullptr) {
        written = write(req->sock_d, resp.body_cstr + resp.body_write_size, lost);
    } else if (!resp.body_str.empty()) {
        written = write(req->sock_d, resp.body_str.data() + resp.body_write_size, lost);
    } else if (!resp.body_file_path.empty()) {
        written = sendfile(req->sock_d, resp.body_fd, &resp.body_file_offset, lost);
    } else {
        assert(1);
    }

    if (written > 0) {
        resp.body_write_size += static_cast<size_t>(written);
        return written;
    } else if (written == -1 && errno == EAGAIN) {
        written = 0;
    } else {
        perror("write response");
        written = -1;
    }

    return written;
}

void worker::go_final_success(request* req) noexcept
{
    req->state = request_state::write_response;
    req->wait_state = request_wait_state::wait_none;
    req->request_handler(req);
}

void worker::go_final_error(request* req, int code, const std::string& error) noexcept
{
    req->resp.code = code;
    req->state = request_state::write_response;
    req->wait_state = request_wait_state::wait_none;
    LOG(INFO) << error;
}

// TODO: method for every state
void worker::go_next(request* req, const char* buff, size_t size) noexcept
{
    // TODO: for every step we need to define max buff for safety from overflow
    switch (req->state) {
    case request_state::read_line: {
        switch (req->line.state) {
        case request_line_state::read_method: {
            req->line.method = request_helper::str_to_request_method(buff, size);

            if (req->line.method == request_line_method::none) {
                go_final_error(req, 400, "invalid method type");
                return;
            }

            req->line.state = request_line_state::read_uri;

            req->wait_state = request_wait_state::wait_sp;
            break;
        }

        case request_line_state::read_uri: {
            if (size > 200) {
                go_final_error(req, 414, "max uri size is 200");
                return;
            }

            handle_res res;
            res.type = handle_res_type::ignore;
            if (req->uri_handler) {
                uri u(buff,size);
                if (u.is_valid()) {
                    res = req->uri_handler(req, u);
                } else {
                    go_final_error(req, 400, "bad uri");
                }
            }

            if (res.type == handle_res_type::ignore) {
                req->line.uri.str = std::string(buff, size);
            } else if (res.type == handle_res_type::error) {
                go_final_error(req, res.code, "user handle uri error");
                return;
            }

            req->line.state = request_line_state::read_version;

            req->wait_state = request_wait_state::wait_crlf;

            break;
        }

        case request_line_state::read_version: {
            req->line.version.str = std::string(buff, size);

            req->state = request_state::read_headers;
            req->wait_state = request_wait_state::wait_crlf;
            break;
        }
        }
        break;
    }

    case request_state::read_headers: {
        if (size > 0) {
            auto [key, value] = parse_header(buff, size);
            // TODO: also add content-length to common header list
            if (key.size() == 14 && strncasecmp(key.data(), "Content-Length", key.size()) == 0) {
                bool ok = false;
                int64_t length = value.to_int(ok);
                if (ok && length>=0) {
                    req->body.wait_size = static_cast<size_t>(length);
                } else {
                    go_final_error(req, 400, "invalid content-length");
                }
            } else {
                if (!key.empty()) {
                    handle_res res;
                    res.type = handle_res_type::ignore;
                    if (req->header_handler) {
                        res = req->header_handler(req, key, value);
                    }

                    if (res.type == handle_res_type::ignore) {
                        req->headers.map.insert({
                            std::string(key.data(), key.size()),
                            std::string(value.data(), value.size())
                        });
                    } else if (res.type == handle_res_type::error) {
                        go_final_error(req, res.code, "user handle header error");
                    }
                } else {
                    go_final_error(req, 400, "invalid header format");
                }
            }
        } else {
            switch (req->line.method) {
            case request_line_method::post:
                delete[] req->buff;
                if (req->body.wait_size > 0) {
                    req->buff = new char[req->body.wait_size];
                    req->buff_size = req->body.wait_size;
                    req->buff_head = 0;
                    req->buff_written_size = 0;

                    req->state = request_state::read_body;
                    req->wait_state = request_wait_state::wait_all;
                } else {
                    go_final_error(req, 400, "not found Content-Length header for post method");
                }
                break;
            case request_line_method::get:
                if (req->body.wait_size == 0) {
                    go_final_success(req);
                } else {
                    go_final_error(req, 400, "get can't contain body");
                }
                break;
            case request_line_method::none:
                assert(false);
                break;
            }
        }
        break;
    }

    case request_state::read_body: {
        if (req->buff_written_size >= req->body.wait_size) {
            req->body.buff = req->buff;
            req->body.buff_size = req->buff_written_size;

            req->buff = nullptr;
            req->buff_size = 0;
            req->buff_head = 0;
            req->buff_written_size = 0;

            go_final_success(req);
        }
        break;
    }

    case request_state::write_response: {
        break;
    }

    }
}

void worker::handle_in(request* req) noexcept
{
    if (req->state == request_state::write_response) {
        // TODO: is request are coming here
        return;
    }

    // size separators like sp(space), crlf(\r\n)
    size_t ignore_size = 0;

    // TODO we can allocate for reqeust meta info in constructor without dynamic alloc
    if (req->buff == nullptr) {
        req->buff = new char[2048];
        if (req->buff == nullptr) {
            // couldn't allocate memory
        }
        req->buff_size = 2048;
    }

    ssize_t avaible_size = static_cast<ssize_t>(req->buff_size) - static_cast<ssize_t>(req->buff_written_size);
    if (avaible_size <= 0) {
        go_final_error(req, 400, "too big request");
        return;
    }

    size_t current_pos = req->buff_written_size;
    ssize_t s_read_size = read(req->sock_d, req->buff + current_pos, static_cast<size_t>(avaible_size));
    if (s_read_size <= 0) {
        return;
    }
    size_t read_size = static_cast<size_t>(s_read_size);
    req->buff_written_size += read_size;

    for (; current_pos<req->buff_written_size; ++current_pos) {
        if (req->wait_state == request_wait_state::wait_sp) {
            if (req->buff[current_pos] == 0x20) {
                ignore_size = 1;
                req->need_process = true;
            }
        } else if (req->wait_state == request_wait_state::wait_crlf) {
            if (!req->got_cr) {
                if (req->buff[current_pos] == 0x0D) {
                    req->got_cr = true;
                    continue;
                }
            } else {
                if (req->buff[current_pos] == 0x0A) {
                    ignore_size = 2;
                    req->need_process = true;
                } else {
                    // TODO: go_final_error. Ignore when parsing body.
                    // If we parse body we have wail_all state
                    req->got_cr = false;
                }
            }
        } else if (req->wait_state == request_wait_state::wait_all) {
            char* buff = req->buff + req->buff_head;
            size_t size = req->buff_written_size - (req->buff_head + 1);
            go_next(req, buff, size);
            req->buff_head += size;
            break;
        } else if (req->wait_state == request_wait_state::wait_none) {
            break;
        }

        if (req->need_process) {
            char* buff = req->buff + req->buff_head;
            // +1 because all positions start with 0
            size_t size = (current_pos - req->buff_head) + 1;

            go_next(req, buff, size - ignore_size);

            req->got_sp = false;
            req->got_cr = false;
            req->got_lf = false;
            req->need_process = false;

            req->buff_head += size;
        }
    }
}

void worker::handle_out(request* req) noexcept
{
    if (req->state != request_state::write_response) {
        return;
    }

    response& resp = req->resp;

    if (resp.line.empty()) {
        // define body size for cxxstring or file
        // cstr size must be defined by user

        if (!resp.body_file_path.empty() && resp.body_fd == -1) {
            resp.body_fd = open(resp.body_file_path.data(), O_RDONLY);
            if (resp.body_fd == -1) {
                perror("open file to write body");
                release_request(req);
                return;
            }

            ssize_t size = filesystem::file_size(resp.body_fd);
            if (size != -1) {
                resp.body_size = static_cast<size_t>(size);
            } else {
                release_request(req);
                return;
            }
        } else if (!resp.body_str.empty()) {
            resp.body_size = resp.body_str.size();
        }

        std::stringstream ss;
        ss << "HTTP/1.1 " << resp.code << " " << request_helper::status_code_to_str(resp.code) << "\r\n";
        if (resp.body_size > 0) {
            ss << "Content-Length: " << resp.body_size << "\r\n";
        }
        ss << "\r\n";
        resp.line = ss.str();
    }

    size_t lost = resp.line.size() - resp.line_write_size;
    if (lost > 0) {
        ssize_t written = write(req->sock_d, resp.line.c_str() + resp.line_write_size, lost);
        if (written > 0) {
            resp.line_write_size += static_cast<size_t>(written);
            return;
        } else if (written == -1 && errno == EAGAIN) {
            return;
        } else {
            perror("write response line");
            release_request(req);
            return;
        }
    }

    if (resp.line_write_size >= resp.line.size()) {
        if (write_response_body(req) == -1) {
            release_request(req);
            return;
        }
    }

    if (resp.body_write_size >= resp.body_size) {
        release_request(req);
    }
}

void worker::loop() noexcept
{
    while (_isRuning) {
        int ready_desc = epoll_wait(_epoll_d, events, max_events, timeout_msecs);
        for (int i = 0; i < ready_desc; ++i) {
            const epoll_event& event = events[i];
            request* req = reinterpret_cast<request*>(event.data.ptr);
            if (event.events&EPOLLRDHUP) {
                release_request(req);
            } else if (event.events&EPOLLIN) {
                handle_in(req);
                if (req->state == request_state::write_response) {
                    epoll_event event;
                    event.events = EPOLLOUT | EPOLLRDHUP;
                    event.data.ptr = req;
                    if (epoll_ctl(_epoll_d, EPOLL_CTL_MOD, req->sock_d, &event) == -1) {
                        perror("epoll_ctl mod");
                    }
                }
            } else if (event.events&EPOLLOUT) {
                handle_out(req);
            } else {
                assert(false);
            }
        }
    }
}

std::pair<http::string, http::string> worker::parse_header(const char* buff, size_t size) noexcept
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
