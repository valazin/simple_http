#ifndef APPLICATION_H
#define APPLICATION_H

#include <string>
#include <map>
#include <deque>
#include <memory>

#include "http/string.h"
#include "http/request.h"

namespace http {
class request;
class server;
}

struct chunk;
struct playlist;

class application
{
public:
    application() noexcept;
    ~application();

    void start() noexcept;

private:
    http::handle_res handle_uri(http::request* req, http::string uri) noexcept;
    http::handle_res handle_header(http::request* req, http::string key, http::string value) noexcept;
    void handle_request(http::request* req) noexcept;

    bool post_chunk(const std::string& pls_id, std::shared_ptr<chunk> cnk) noexcept;
    bool get_chunk(const std::string& pls_id, int64_t seq, http::request* req) noexcept;
    bool get_playlist(const std::string& pls_id, http::request* req) noexcept;

    std::string build_playlist(const std::string& pls_id, playlist* plst) noexcept;
    std::string chunk_url(const std::string& pls_id, std::shared_ptr<chunk> cnk) const noexcept;

private:
    std::map<std::string, playlist*> _playlists;
    std::unique_ptr<http::server> _server;

    std::string _hostname;
};

#endif // APPLICATION_H
