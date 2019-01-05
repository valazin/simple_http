#include "worker.h"

#include "request.h"

#include <unistd.h>

#include <cstring>
#include <cassert>
#include <iostream>
#include <sstream>

using namespace http;

worker::worker(int epoll_d) :
    _epoll_d(epoll_d)
{
    _isRuning.store(false);
}

worker::~worker()
{
    if (_thread.joinable()) {
        _thread.join();
    }
}

void worker::start()
{
    _isRuning.store(true);
    _thread = std::thread(&worker::loop, this);
}

void worker::stop()
{
    _isRuning.store(false);
}

void worker::go_final_success(request *req)
{
    req->body.buff = req->buff;
    req->body.size = req->buf_size;
    req->buff_free_after_close = false;

    req->state = request_state::write_response;
    req->wait_state = request_wait_state::wait_none;

    req->handler(req);
}

void worker::go_final_error(request *req, int code, const std::string &error)
{
    req->buff_free_after_close = true;

    req->resp.code = code;
    req->state = request_state::write_response;
    req->wait_state = request_wait_state::wait_none;
    std::cout << error << std::endl << std::flush;
}

void worker::go_next(request *req, const char *buff, size_t size)
{
    switch (req->state) {
    case request_state::read_line: {
        switch (req->line.state) {
        case request_line_state::read_method:
            req->line.method = request_helper::str_to_request_method(buff, size);

            if (req->line.method == request_line_method::none) {
                go_final_error(req, 400, "invalid method type");
                return;
            }

            req->line.state = request_line_state::read_uri;

            req->wait_state = request_wait_state::wait_sp;
            break;
        case request_line_state::read_uri:
            req->line.uri.str = std::string(buff, size);
            req->line.state = request_line_state::read_version;

            req->wait_state = request_wait_state::wait_crlf;
            break;
        case request_line_state::read_version:
            req->line.version.str = std::string(buff, size);

            req->state = request_state::read_headers;
            req->wait_state = request_wait_state::wait_crlf;
            break;
        }
        break;
    }

    case request_state::read_headers: {
        if (size > 0) {
            // optimize: interested only in "Content-Length" header
            const size_t key_size = 14;
            if (size > key_size && strncasecmp(buff, "Content-Length", key_size) == 0) {
                size_t value_pos = key_size + 1;
                for (; value_pos<size; ++value_pos) {
                    if (buff[value_pos] != 0x20 && buff[value_pos] != ':') {
                        break;
                    }
                }
                size_t value_size = size - value_pos;
                if (value_size > 0) {
                    int value = 0;
                    bool was_converted = true;
                    for (size_t i=value_pos; i<size; ++i) {
                        if (buff[i] >= 0x30 && buff[i] <= 0x39) {
                            value = value*10 + buff[i] - '0';
                        } else {
                            was_converted = false;
                            break;
                        }
                    }

                    if (was_converted) {
                        req->body.body_size = static_cast<size_t>(value);
                    } else {
                        go_final_error(req, 400, "invalid content-length");
                    }
                }
            } else {
                auto pair = parse_header(buff, size);
                if (!pair.first.empty()) {
                    req->headers.map.insert(pair);
                }
            }
        } else {
            switch (req->line.method) {
            case request_line_method::post:
                if (req->body.body_size > 0) {
                    req->state = request_state::read_body;
                    req->wait_state = request_wait_state::wait_all;
                } else {
                    go_final_error(req, 400, "not found Content-Length for post method");
                }
                break;
            case request_line_method::get:
                if (req->body.body_size == 0) {
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
        if (request_helper::request_buff_append(req, buff, size)) {
            req->body.write_size += size;
        } else {
            go_final_error(req, 500, "coundn't save body");
            return;
        }

        if (req->body.write_size >= req->body.body_size) {
            go_final_success(req);
        }
        break;
    }

    case request_state::write_response: {
        break;
    }

    }
}

void worker::handle_in(request *req)
{
    if (req->state == request_state::write_response) {
        return;
    }

    size_t sp_size = 0;

    ssize_t s_read_size = read(req->sock_d, &in_buff, in_buff_size);
    if (s_read_size <= 0) {
        return;
    }
    size_t read_size = static_cast<size_t>(s_read_size);

    size_t current_pos = 0;
    for (size_t i=0; i<read_size; ++i) {
        if (req->wait_state == request_wait_state::wait_sp) {
            if (in_buff[i] == 0x20) {
                sp_size = 1;
                req->need_process = true;
            }
        } else if (req->wait_state == request_wait_state::wait_crlf) {
            if (!req->got_cr) {
                if (in_buff[i] == 0x0D) {
                    req->got_cr = true;
                    continue;
                }
            } else {
                if (in_buff[i] == 0x0A) {
                    sp_size = 2;
                    req->need_process = true;
                } else {
                    // ERROR
                    req->got_cr = false;
                }
            }
        } else if (req->wait_state == request_wait_state::wait_all) {
            break;
        } else if (req->wait_state == request_wait_state::wait_none) {
            break;
        }

        if (req->need_process) {
            char* buff = in_buff + current_pos;
            size_t size = (i - current_pos) + 1;

            if (req->buff == nullptr) {
                go_next(req, buff, size - sp_size);
            } else {
                if (request_helper::request_buff_append(req, buff, size - sp_size)) {
                    go_next(req, req->buff, req->buf_size);
                } else {
                    go_final_error(req, 500, "request buffer append");
                    break;
                }
            }

            current_pos += size;

            req->got_sp = false;
            req->got_cr = false;
            req->got_lf = false;
            req->need_process = false;
        }
    }

    if (req->wait_state != request_wait_state::wait_none && current_pos+1 < read_size) {
        char* buff = in_buff + current_pos;
        size_t size = read_size - current_pos;

        if (req->wait_state == request_wait_state::wait_all) {
            go_next(req, buff, size);
        } else {
            if (!request_helper::request_buff_append(req, buff, size)) {
                go_final_error(req, 500, "buffer append to request");
            }
        }
    }
}

void worker::handle_out(request *req)
{
    if (req->state != request_state::write_response) {
        return;
    }

    if (req->resp.line.empty()) {
        std::stringstream ss;
        ss << "HTTP/1.1 " << req->resp.code << " " << request_helper::status_code_to_str(req->resp.code) << "\r\n";
        if (req->resp.body != nullptr) {
            ss << "Content-Length: " << req->resp.body_size << "\r\n";
        }
        ss << "\r\n";
        req->resp.line = ss.str();
    }

    size_t lost = req->resp.line.size() - req->resp.line_write_size;
    if (lost > 0) {
        ssize_t written = write(req->sock_d, req->resp.line.c_str() + req->resp.line_write_size, lost);
        if (written > 0) {
            req->resp.line_write_size += static_cast<size_t>(written);
        } else {
            perror("write response line");
        }
    }

    if (req->resp.line_write_size >= req->resp.line.size()) {
        size_t lost = (req->resp.body_size - req->resp.body_write_size);
        if (lost > 0) {
            ssize_t written = write(req->sock_d, req->resp.body + req->resp.body_write_size, lost);
            if (written > 0) {
                req->resp.body_write_size += static_cast<size_t>(written);
            } else {
                perror("write response body");
            }
        }

        if (req->resp.body_write_size >= req->resp.body_size) {
            close(req->sock_d);
            if (req->resp.body != nullptr) {
                free(req->resp.body);
            }
            if (req->buff_free_after_close) {
                free(req->buff);
            }
            free(req);
        }
    }
}

void worker::loop()
{
    while (_isRuning) {
        int ready_desc = epoll_wait(_epoll_d, events, max_events, timeout_msecs);
        for (int i = 0; i < ready_desc; ++i) {
            const epoll_event event = events[i];
            request* req = reinterpret_cast<request*>(event.data.ptr);
            if (event.events&EPOLLIN) {
                handle_in(req);
                if (req->state == request_state::write_response) {
                    epoll_event* event = reinterpret_cast<epoll_event*>(malloc(sizeof(epoll_event)));
                    if (!event) {
                        perror("allocate memory for epoll out event");
                        continue;
                    }
                    event->events = EPOLLOUT;
                    event->data.ptr = req;
                    if (epoll_ctl(_epoll_d, EPOLL_CTL_MOD, req->sock_d, event) == -1) {
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

std::pair<std::string, std::string> worker::parse_header(const char *buff, size_t size)
{
    std::pair<std::string, std::string> res;

    size_t key_size = 0;
    for (; key_size<size; ++key_size) {
        if (buff[key_size] == 0x20 || buff[key_size] == ':') {
            break;
        }
    }

    if (key_size == 0) {
        return res;
    }

    res.first = std::string(buff, key_size);

    size_t value_pos = key_size + 1;
    for (; value_pos<size; ++value_pos) {
        if (buff[value_pos] != 0x20 && buff[value_pos] != ':') {
            break;
        }
    }

    size_t value_size = size - value_pos;
    if (value_size <= 0) {
        return res;
    }

    res.second = std::string(buff+value_pos, value_size);

    return res;
}

// TODO:
//int worker::buff_to_int(const char *buff, size_t size)
//{

//}
