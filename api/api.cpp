#include "api.h"

#include <cstring>
#include <iostream>

#include "hls_context.h"
#include "../utility/filesystem.h"
#include "../http/server.h"
#include "../hls/chunk.h"
#include "../hls_live_storage/hls_live_storage.h"
#include "../hls_archive_storage/hls_arhive_storage.h"

api::api(hls_live_storage *live_storage, hls_archive_storage* archive_storage) :
    _live_storage(live_storage),
    _archive_storage(archive_storage)
{
    _server = std::make_unique<http::server>();
}

api::~api()
{
    _server->stop();
}

void api::start(const std::string &host, uint16_t port) noexcept
{
    _server->start(host, port,
                   std::bind(&api::handle_request, this, std::placeholders::_1),
                   std::bind(&api::handle_uri, this, std::placeholders::_1, std::placeholders::_2),
                   std::bind(&api::handle_header, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

http::handle_res api::handle_uri(http::request *req, http::uri uri) noexcept
{
    hls_context* cxt = new hls_context;
    if (cxt == nullptr) {
        std::cerr << "FATAL error: couldn't allocate memory for hls context" << std::endl;
        return {http::handle_res_type::error, 500};
    }

    http::handle_res res = fetch_hls_context_from_uri(req->line.method, uri, cxt);
    if (res.type == http::handle_res_type::success
            || res.type == http::handle_res_type::ignore) {
        req->user_data = cxt;
    } else {
        delete cxt;
    }

    return res;
}

http::handle_res api::handle_header(http::request *req,
                                    http::string key,
                                    http::string value) noexcept
{
    hls_context* cxt = reinterpret_cast<hls_context*>(req->user_data);
    if (!cxt) {
        delete cxt;
        std::cerr << "FATAL error: couldn't fetch hls context from request" << std::endl;
        return {http::handle_res_type::error, 500};
    }

    http::handle_res res = fetch_hls_context_from_header(req->line.method, key, value, cxt);
    if (res.type == http::handle_res_type::error) {
        delete cxt;
    }

    return res;
}

void api::handle_request(http::request *req) noexcept
{
    hls_context* cxt = reinterpret_cast<hls_context*>(req->user_data);
    if (!cxt) {
        req->resp.code = 500;
        delete cxt;
        return;
    }

    switch(cxt->method) {
    case hls_method::post_chunk: {

        auto cnk = std::make_shared<chunk>();
        cnk->seq = cxt->seq;
        cnk->start_ut_msecs = cxt->start_ut_msecs;
        cnk->duration_msecs = cxt->duration_msecs;

        // TODO: how to handle error

        if (_live_storage) {
            if (_live_storage->add_chunk(cxt->hls_id, cnk)) {
                req->resp.code = 200;

                cnk->buff = req->body.buff;
                cnk->size = req->body.buff_size;

                req->body.buff = nullptr;
                req->body.buff_size = 0;
            } else {
                req->resp.code = 500;
            }
        }

        if (_archive_storage) {
            _archive_storage->add_chunk(cxt->hls_id, cnk);
        }

        break;
    }

    case hls_method::get_live_chunk: {
        auto cnk = _live_storage->get_chunk(cxt->hls_id, cxt->seq);
        if (cnk != nullptr) {
            req->resp.code = 200;
            req->resp.body = cnk->buff;
            req->resp.body_size = cnk->size;
            req->resp.free_body = false;
        } else {
            req->resp.code = 404;
        }
        break;
    }

    case hls_method::get_live_playlist: {
        std::string txt = _live_storage->get_playlist(cxt->hls_id);
        if (!txt.empty()) {
            req->resp.code = 200;
            req->resp.body = reinterpret_cast<char*>(malloc(txt.size()));
            memcpy(req->resp.body, txt.data(), txt.size());
            req->resp.body_size = txt.size();
//            req->resp.free_body = false;
        } else {
            req->resp.code = 500;
        }
        break;
    }

    case hls_method::get_archive_chunk: {

        break;
    }

    case hls_method::get_archive_playlist: {

        break;
    }

    case hls_method::undefined: {
        req->resp.code = 500;
        break;
    }
    }

    delete cxt;
}

http::handle_res api::fetch_hls_context_from_uri(http::request_line_method method,
                                                 http::uri uri,
                                                 hls_context *cxt) const noexcept
{
    // hls/:id/live/index.m3u8
    // hls/:id/live/1.ts
    // hls/:id/archive/index.m3u8?start=1000&duration=2000&
    // hls/:id/arhive/year/month/day/1.ts

    auto path_items = uri.get_path_items();

    switch (method) {
    case http::request_line_method::post: {
        if (path_items.size() == 1) {
            if (path_items.at(0).compare("files") == 0) {
                cxt->method = hls_method::post_chunk;
                return {http::handle_res_type::success};
            }
        }
        break;
    }

    case http::request_line_method::get: {
        if (path_items.size() < 4 || path_items.size() > 7) {
            break;
        }

        if (path_items[0].compare("hls") != 0) {
            return {http::handle_res_type::error};
        }
        cxt->hls_id = std::string(path_items[1].data(), path_items[1].size());

        if (path_items[2].compare("live") == 0) {
            if (!_live_storage) {
                return {http::handle_res_type::error, 404};
            }

            http::string file = path_items[3];
            http::string name = file.cut_by('.');
            if (file.compare("ts") == 0) {
                bool ok = false;
                cxt->seq = name.to_int(ok);
                if (ok) {
                    cxt->method = hls_method::get_live_chunk;
                    return {http::handle_res_type::success};
                }
            } else if (file.compare("m3u8") == 0) {
                cxt->method = hls_method::get_live_playlist;
                return {http::handle_res_type::success};
            }
        } else if (path_items[2].compare("archive") == 0) {
            if (!_archive_storage) {
                return {http::handle_res_type::error, 404};
            }

            if (path_items[3].compare("index.m3u8") == 0) {
                cxt->method = hls_method::get_archive_playlist;
                bool ok = false;
                cxt->start_ut_msecs = uri.find_query_item("start").to_int(ok);
                if (!ok) {
                    return {http::handle_res_type::error, 404};
                }
                cxt->duration_msecs = uri.find_query_item("duration").to_int(ok);
                if (!ok) {
                    return {http::handle_res_type::error, 404};
                }
                return {http::handle_res_type::success};
            } else {
                for (size_t i = 3; i < path_items.size(); ++i) {
                    cxt->path += path_items[i].to_str();
                    if (i+1 <path_items.size()) {
                        cxt->path += "/";
                    }
                }
                cxt->method = hls_method::get_archive_chunk;
                return {http::handle_res_type::success};
            }
        }

        break;
    }

    default:
        return {http::handle_res_type::error, 404};
    }

    return {http::handle_res_type::error, 404};
}

http::handle_res api::fetch_hls_context_from_header(http::request_line_method method,
                                                    http::string key,
                                                    http::string value,
                                                    hls_context *cxt) const noexcept
{
    if (method != http::request_line_method::post) {
        return {http::handle_res_type::ignore};
    }

    if (cxt->hls_id.empty() && key.compare("X-HLS-ID") == 0) {
        cxt->hls_id = std::string(value.data(), value.size());
        return {http::handle_res_type::success};
    } else if (cxt->start_ut_msecs == -1 && key.compare("X-HLS-TIMESTAMP") == 0) {
        bool ok = false;
        cxt->start_ut_msecs = value.to_int(ok);
        if (ok) {
            return {http::handle_res_type::success};
        } else {
            return {http::handle_res_type::error, 404};
        }
    } else if (cxt->duration_msecs == -1 && key.compare("X-HLS-DURATION") == 0) {
        bool ok = false;
        cxt->duration_msecs = value.to_int(ok);
        if (ok) {
            return {http::handle_res_type::success};
        } else {
            return {http::handle_res_type::error, 404};
        }
    } else if (cxt->seq == -1 && key.compare("X-HLS-SEQ") == 0) {
        bool ok = false;
        cxt->seq = value.to_int(ok);
        if (ok) {
            return {http::handle_res_type::success};
        } else {
            return {http::handle_res_type::error, 404};
        }
    }

    return {http::handle_res_type::ignore};
}
