#ifndef API_H
#define API_H

#include <string>
#include <memory>

#include "../http/string.h"
#include "../http/request.h"
#include "../hls_live_storage/hls_live_storage_error.h"

namespace http {
class server;
}

namespace hls_live {
class storage;
}

class hls_archive_storage;

class hls_context;

class api
{
public:
    api(hls_live::storage* live_storage, hls_archive_storage* archive_storage);
    ~api();

    void start(const std::string& host, unsigned int port) noexcept;

private:
    http::handle_res handle_uri(http::request* req, http::uri uri) noexcept;
    http::handle_res handle_header(http::request* req, http::string key, http::string value) noexcept;
    void handle_request(http::request* req) noexcept;

    http::handle_res fetch_hls_context_from_uri(http::request_line_method method, http::uri uri, hls_context* cxt) const noexcept;
    http::handle_res fetch_hls_context_from_header(http::request_line_method method, http::string key, http::string value, hls_context* cxt) const noexcept;

    static int hls_live_error_to_rest_code(hls_live::error_type error) noexcept;

private:
    hls_live::storage* _live_storage = nullptr;
    hls_archive_storage* _archive_storage = nullptr;
    std::unique_ptr<http::server> _server;
};

#endif // API_H
