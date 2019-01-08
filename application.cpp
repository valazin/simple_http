#include "application.h"

#include <iostream>
#include <sstream>
#include <cstring>

#include "http/server.h"

enum class hls_method
{
    undefined,
    post_chunk,
    get_chunk,
    get_playlist
};

struct hls_context
{
    std::string hls_id;
    int64_t seq = -1;
    int64_t start_unix_timestamp = -1;
    int64_t duration_msecs = -1;

    hls_method method = hls_method::undefined;
};

struct chunk
{
    ~chunk()
    {
        if (buff != nullptr) {
            free(buff);
        }
    }

    char* buff = nullptr;
    size_t size;
    int64_t seq = -1;
    int64_t start_unix_timestamp = 0;
    int64_t durationMsecs = 0;
};

struct playlist
{
    size_t max_size = 20;
    size_t live_size = 5;
    std::deque<std::shared_ptr<chunk>> chunks;

    std::mutex mtx;
};

application::application()
{
    _server = std::make_unique<http::server>();
}

application::~application()
{
    _server->stop();

    for (auto&& pair : _playlists) {
        delete pair.second;
    }
}

void application::start()
{
    const std::string host = "127.0.0.1";
    const uint16_t port = 1024;

    _hostname = host + ":" + std::to_string(port);

    _server->start(host, port,
                   std::bind(&application::handle_request, this, std::placeholders::_1),
                   std::bind(&application::handle_uri, this, std::placeholders::_1, std::placeholders::_2),
                   std::bind(&application::handle_header, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

http::handle_res application::handle_uri(http::request* req, http::string uri)
{
    // TODO: free memory
    hls_context* cxt = new hls_context;
    if (!cxt) {
        return {500, "couldn't allocate memory for hls context", http::handle_res_type::error};
    }

    uri.cut_by('/');

    // http://hostname:port/hls/id
    // http://hostname:port/hls/id/1

    if (req->line.method == http::request_line_method::post) {
        if (uri.size() == 5 && strncmp(uri.data(), "chunk", 5) == 0) {
            cxt->method = hls_method::post_chunk;

            req->user_data = cxt;

            return {0, "", http::handle_res_type::success};
        } else {
            return {404, "not found supported method while parsing uri", http::handle_res_type::error};
        }
    } else if (req->line.method == http::request_line_method::get) {
        http::string controller = uri.cut_by('/');
        if (controller.size() != 3 || strncmp(controller.data(), "hls", controller.size()) != 0) {
            return {404, "not found supported controller while parsing uri", http::handle_res_type::error};
        }

        http::string head = uri.cut_by('/');
        if (!head.empty()) {
            cxt->hls_id = std::string(head.data(), head.size());
        } else if (!uri.empty()) {
            cxt->hls_id = std::string(uri.data(), uri.size());
            cxt->method = hls_method::get_playlist;

            req->user_data = cxt;

            return {0, "", http::handle_res_type::success};
        } else {
            return {404, "not found supported method for hls controller while parsing uri", http::handle_res_type::error};
        }

        head = uri.cut_by('/');
        if (head.empty()) {
            bool ok = false;
            cxt->seq = uri.to_int(ok);
            if (ok) {
                cxt->method = hls_method::get_chunk;

                req->user_data = cxt;

                return {0, "", http::handle_res_type::success};
            } else {
                return {400, "couldn't fetch chunk seq while parsing uri", http::handle_res_type::error};
            }
        } else {
            return {404, "uri is ubigutious", http::handle_res_type::error};
        }
    } else {
        return {404, "unsupported http method", http::handle_res_type::error};
    }
}

http::handle_res application::handle_header(http::request* req, http::string key, http::string value)
{
    if (req->line.method == http::request_line_method::post) {
        hls_context* cxt = reinterpret_cast<hls_context*>(req->user_data);
        if (!cxt) {
            return {500, "couldn't get context while parsing header", http::handle_res_type::error};
        }

        // TODO: check header size or make compare method in http::stirng

        if (cxt->hls_id.empty() && strncmp(key.data(), "X-HLS-ID", key.size()) == 0) {
            cxt->hls_id = std::string(value.data(), value.size());
            return {0, "", http::handle_res_type::success};
        } else if (cxt->start_unix_timestamp == -1 && strncmp(key.data(), "X-HLS-TIMESTAMP", key.size()) == 0) {
            bool ok = false;
            cxt->start_unix_timestamp = value.to_int(ok);
            if (ok) {
                return {0, "", http::handle_res_type::success};
            } else {
                return {400, "couldn't fetch int from X-HLS-TIMESTAMP", http::handle_res_type::error};
            }
        } else if (cxt->duration_msecs == -1 && strncmp(key.data(), "X-HLS-DURATION", key.size()) == 0) {
            bool ok = false;
            cxt->duration_msecs = value.to_int(ok);
            if (ok) {
                return {0, "", http::handle_res_type::success};
            } else {
                return {400, "couldn't fetch int from X-HLS-DURATION", http::handle_res_type::error};
            }
        } else if (cxt->seq == -1 && strncmp(key.data(), "X-HLS-SEQ", key.size()) == 0) {
            bool ok = false;
            cxt->seq = value.to_int(ok);
            if (ok) {
                return {0, "", http::handle_res_type::success};
            } else {
                return {400, "couldn't fetch int from X-HLS-SEQ", http::handle_res_type::error};
            }
        } else {
            return {0, "", http::handle_res_type::ignore};
        }
    } else {
        return {0, "", http::handle_res_type::ignore};
    }
}

void application::handle_request(http::request *req)
{
    std::cout << "handle " << req->sock_d << std::endl << std::flush;

    hls_context* cxt = reinterpret_cast<hls_context*>(req->user_data);
    if (!cxt) {
        req->resp.code = 500;
        return;
    }

    // TODO: use mutex for each playlist
    std::lock_guard<std::mutex> guard(_mtx);

    switch(cxt->method) {
    case hls_method::post_chunk: {
        if (cxt->hls_id.empty()
                || cxt->seq == -1
                || cxt->start_unix_timestamp == -1
                || cxt->duration_msecs == -1 ) {
            req->resp.code = 400;
            return;
        }

        auto cnk = std::make_shared<chunk>();
        cnk->seq = cxt->seq;
        cnk->start_unix_timestamp = cxt->start_unix_timestamp;
        cnk->durationMsecs = cxt->duration_msecs;
        if (post_chunk(cxt->hls_id, cnk)) {
            req->resp.code = 200;

            cnk->buff = req->body.buff;
            cnk->size = req->body.buff_size;
            req->body.buff = nullptr;
            req->body.buff_size = 0;
        } else {
            req->resp.code = 500;
        }
        break;
    }

    case hls_method::get_chunk: {
        if (get_chunk(cxt->hls_id, cxt->seq, req)) {
            req->resp.code = 200;
        } else {
            req->resp.code = 500;
        }
        break;
    }

    case hls_method::get_playlist: {
        if (get_playlist(cxt->hls_id, req)) {
            req->resp.code = 200;
        } else {
            req->resp.code = 500;
        }
        break;
    }

    case hls_method::undefined: {
        req->resp.code = 500;
        break;
    }
    }
}

bool application::post_chunk(const std::string &pls_id,
                            std::shared_ptr<chunk> cnk)
{
    std::cout << "post_chunk " << pls_id << " "
              << cnk->seq << " "
              << cnk->start_unix_timestamp << " "
              << cnk->durationMsecs
              << std::endl << std::flush;

    playlist* plst = nullptr;

    auto searched = _playlists.find(pls_id);
    if (searched == _playlists.end()) {
        plst = new playlist;
        _playlists.insert({pls_id, plst});
    } else {
        plst = searched->second;
    }

    if (plst->chunks.empty()) {
        plst->chunks.push_back(cnk);
    } else {
        auto last_cnk = plst->chunks.back();
        int64_t back_gap = cnk->seq - last_cnk->seq;
        if (back_gap < 0) {
            // put before end
            auto first_cnk = plst->chunks.front();
            int64_t front_gap = first_cnk->seq - cnk->seq;
            if (front_gap > 0) {
                // put before first
                int64_t avaible = static_cast<int64_t>(plst->max_size - plst->chunks.size());
                if (avaible > front_gap) {
                    // fill gap
                    for (int64_t i=front_gap-1; i>0; --i) {
                        auto dummy = std::make_shared<chunk>();
                        dummy->seq = cnk->seq + i;
                        plst->chunks.push_front(dummy);
                    }
                    plst->chunks.push_front(cnk);
                }
            } else {
                // put into the middle
                auto i = plst->chunks.end() - (-1*back_gap) - 1;
                (*i) = cnk;
            }
        } else if (back_gap == 0) {
            // ignore
        } else if (back_gap >= 1) {
            // put after end
            int64_t avaible = static_cast<int64_t>(plst->max_size - plst->chunks.size());
            if (avaible > back_gap) {
                // fill gap
                for (int64_t i=0; i<back_gap-1; ++i) {
                    auto dummy = std::make_shared<chunk>();
                    dummy->seq = (last_cnk->seq + 1) + i;
                    plst->chunks.push_back(dummy);
                }
            } else {
                plst->chunks.clear();
            }

            plst->chunks.push_back(cnk);
        }

        int64_t diff = static_cast<int64_t>(plst->chunks.size() - plst->max_size);
        if (diff > 0) {
            plst->chunks.erase(plst->chunks.cbegin(), plst->chunks.cbegin() + diff);
        }
    }

    return true;
}

bool application::get_chunk(const std::string &pls_id, int64_t seq, http::request *req)
{
    std::cout << "get_chunk " << pls_id << " " << seq << std::endl << std::flush;

    playlist* plst = nullptr;

    auto searched = _playlists.find(pls_id);
    if (searched == _playlists.end()) {
        return false;
    }
    plst = searched->second;

    if (!plst->chunks.empty()) {
        for (auto&& cnk : plst->chunks) {
            if (cnk->seq == seq) {
                if (cnk->buff != nullptr) {
                    req->resp.free_body = false;
                    req->resp.body = cnk->buff;
                    req->resp.body_size = cnk->size;
                    return true;
                }
            }
        }
    }

    return false;
}

bool application::get_playlist(const std::string &pls_id, http::request *req)
{
    std::cout << "get_playlist " << pls_id << std::endl << std::flush;

    playlist* plst = nullptr;

    auto searched = _playlists.find(pls_id);
    if (searched == _playlists.end()) {
        return false;
    }
    plst = searched->second;

    if (plst->chunks.empty()) {
        return false;
    }

    auto i = plst->chunks.begin();
    if (plst->chunks.size() > plst->live_size) {
        i -= static_cast<int64_t>(plst->live_size);
    }

    std::stringstream ss;
    ss << "#EXTM3U" << std::endl;
    ss << "#EXT-X-TARGETDURATION:2" << std::endl;
    ss << "#EXT-X-VERSION:4" << std::endl;
    ss << "#EXT-X-MEDIA-SEQUENCE:" << (*i)->seq << std::endl;
    while (i != plst->chunks.end()) {
        auto cnk = (*i++);
        ss << "#EXTINF:" << ((cnk->durationMsecs / 1000) + 1) << "," << std::endl;
        ss << chunk_url(pls_id, cnk) << std::endl;
    }

    std::string data = ss.str();
    req->resp.body = reinterpret_cast<char*>(malloc(data.size()));
    if (req->resp.body == nullptr) {
        return false;
    }
    memcpy(req->resp.body, data.c_str(), data.size());
    req->resp.body_size = data.size();

    return true;
}

std::string application::chunk_url(const std::string &pls_id, std::shared_ptr<chunk> cnk) const
{
    return std::string(_hostname + "/" + pls_id + "/" + std::to_string(cnk->seq));
}
