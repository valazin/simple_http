#ifndef APPLICATION_H
#define APPLICATION_H

#include <string>
#include <map>
#include <mutex>
#include <deque>
#include <memory>

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
    void handler(http::request* req);
    void handle_get(http::request* req);
    void handle_post(http::request* req);

    bool post_chunk(const std::string& plsId, std::shared_ptr<chunk> cnk);
    bool get_chunk(const std::string& plsId, int64_t seq);
    bool get_playlist(const std::string& plsId, http::request* req);

    static std::string fetch_str_header(http::request* req, const std::string& name, bool& found);
    static int64_t fetch_int_header(http::request* req, const std::string& name, bool& found);

private:
    std::map<std::string, playlist*> _playlists;
    std::unique_ptr<http::server> _server;
    std::mutex _mtx;
};

#endif // APPLICATION_H
