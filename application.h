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
    application();
    ~application();

    void start();

private:
    // TODO: add noexcept
    http::handle_res handle_uri(http::request* req, http::string uri);
    http::handle_res handle_header(http::request* req, http::string key, http::string value);
    void handle_request(http::request* req);

    bool post_chunk(const std::string& pls_id, std::shared_ptr<chunk> cnk);
    bool get_chunk(const std::string& pls_id, int64_t seq, http::request* req);
    bool get_playlist(const std::string& pls_id, http::request* req);

    std::string chunk_url(const std::string& pls_id, std::shared_ptr<chunk> cnk) const;

private:
    std::map<std::string, playlist*> _playlists;
    std::unique_ptr<http::server> _server;

    std::string _hostname;
};

#endif // APPLICATION_H
