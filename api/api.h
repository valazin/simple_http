#ifndef API_H
#define API_H

#include <string>
#include <memory>

#include "../http/string.h"
#include "../http/request.h"

namespace http {
class server;
}
class hls_context;
class hls_live_storage;

class api
{
public:
    api(hls_live_storage* live_storage);
    ~api();

    void start(const std::string& host, uint16_t port) noexcept;

private:
    http::handle_res handle_uri(http::request* req, http::uri uri) noexcept;
    http::handle_res handle_header(http::request* req, http::string key, http::string value) noexcept;
    void handle_request(http::request* req) noexcept;

    http::handle_res fetch_hls_context_from_uri(http::request_line_method method, http::uri uri, hls_context* cxt) const noexcept;
    http::handle_res fetch_hls_context_from_header(http::request_line_method method, http::string key, http::string value, hls_context* cxt) const noexcept;

private:
    hls_live_storage* _live_storage;
    std::unique_ptr<http::server> _server;
};

#endif // API_H
