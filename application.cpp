#include "application.h"

#include <iostream>
#include <sstream>
#include <cstring>

#include "http/request.h"
#include "http/server.h"

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
    _server->start("10.110.3.43", 1024, std::bind(&application::handler, this, std::placeholders::_1));
}

void application::handler(http::request *req)
{
    std::cout << "handle " << req->sock_d << std::endl << std::flush;

    switch (req->line.method) {
    case http::request_line_method::get:
        handle_get(req);
        break;
    case http::request_line_method::post:
        handle_post(req);
        break;
    case http::request_line_method::none:
        req->resp.code = 500;
        break;
    }
}

void application::handle_get(http::request *req)
{
    bool found = false;

    // FIXME: uncomment
//    std::string id = fetch_str_header(req, "X-HLS-ID", found);
//    if (!found) {
//        req->resp.code = 400;
//        return;
//    }

    std::string id = (*_playlists.begin()).first;

    std::lock_guard<std::mutex> guard(_mtx);
    if (get_playlist(id, req)) {
        req->resp.code = 200;
    } else {
        req->resp.code = 500;
    }
}

void application::handle_post(http::request *req)
{
    bool found = false;

    std::string id = fetch_str_header(req, "X-HLS-ID", found);
    if (!found) {
        req->resp.code = 400;
        return;
    }

    auto cnk = std::make_shared<chunk>();
    cnk->start_unix_timestamp = fetch_int_header(req, "X-HLS-TIMESTAMP", found);
    if (!found) {
        req->resp.code = 400;
        return;
    }
    cnk->durationMsecs = fetch_int_header(req, "X-HLS-DURATION", found);
    if (!found) {
        req->resp.code = 400;
        return;
    }
    cnk->seq = fetch_int_header(req, "X-HLS-SEQ", found);
    if (!found) {
        req->resp.code = 400;
        return;
    }

    std::lock_guard<std::mutex> guard(_mtx);
    if (post_chunk(id, cnk)) {
        req->resp.code = 200;

        cnk->buff = req->body.buff;
        cnk->size = req->body.size;

        req->body.buff = nullptr;
        req->body.size = 0;
    } else {
        req->resp.code = 500;
    }

    if (req->body.buff != nullptr) {
        free(req->body.buff);
    }
}

bool application::post_chunk(const std::string &plsId,
                            std::shared_ptr<chunk> cnk)
{
    std::cout << "post_chunk " << plsId << " "
              << cnk->seq << " "
              << cnk->start_unix_timestamp << " "
              << cnk->durationMsecs
              << std::endl << std::flush;

    playlist* plst = nullptr;

    auto searched = _playlists.find(plsId);
    if (searched == _playlists.end()) {
        plst = new playlist;
        _playlists.insert({plsId, plst});
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

bool application::get_playlist(const std::string &id, http::request *req)
{
    std::cout << "get_playlist " << id << std::endl << std::flush;

    playlist* plst = nullptr;

    auto searched = _playlists.find(id);
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

    std::string data;
    std::stringstream ss(data);
    ss << "#EXTM3U" << std::endl;
    ss << "#EXT-X-TARGETDURATION:2" << std::endl;
    ss << "#EXT-X-VERSION:4" << std::endl;
    ss << "#EXT-X-MEDIA-SEQUENCE:" << (*i)->seq;
    while (i != plst->chunks.end()) {
        auto cnk = (*i++);
        ss << "#EXTINF:" << ((cnk->durationMsecs / 1000) + 1) << "," << std::endl;
        ss << id + "/" << cnk->seq << ".ts" << std::endl;
    }

    req->resp.body = reinterpret_cast<char*>(malloc(data.size()));
    if (req->resp.body == nullptr) {
        return false;
    }
    memcpy(req->resp.body, data.c_str(), data.size());
    req->resp.body_size = data.size();

    return true;
}

std::string application::fetch_str_header(http::request *req,
                                          const std::string &name,
                                          bool& found)
{
    auto search = req->headers.map.find(name);
    if (search == req->headers.map.end()) {
        found = false;
        return std::string();
    }

    found = true;

    return search->second;
}

int64_t application::fetch_int_header(http::request *req,
                                      const std::string &name,
                                      bool& found)
{
    int64_t res = 0;

    auto search = req->headers.map.find(name);
    if (search == req->headers.map.end()) {
        found = false;
        return res;
    }

    found = true;

    return std::stoi(search->second);
}
