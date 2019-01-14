#include "api.h"

#include <cstring>
#include <iostream>

#include "hls_context.h"
#include "../http/server.h"
#include "../hls/chunk.h"
#include "../hls_live_storage/hls_live_storage.h"

api::api(hls_live_storage *live_storage)
    : _live_storage(live_storage)
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
        return {500, http::handle_res_type::error};
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
        return {500, http::handle_res_type::error};
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
        cnk->start_unix_timestamp = cxt->start_unix_timestamp;
        cnk->duration_msecs = cxt->duration_msecs;
        if (_live_storage->add_chunk(cxt->hls_id, cnk)) {
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

    case hls_method::get_playlist: {
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

    case hls_method::undefined: {
        req->resp.code = 500;
        break;
    }
    }

    delete cxt;
}

/*
for live:
/hls/:id/live/index.m3u8
/hls/:id/live/1.ts

for arhive:
/hls/:id/archive/index.m3u8&start=&duration=&
/hls/:id/arhive/year/month/day/1.ts
*/

http::handle_res api::fetch_hls_context_from_uri(http::request_line_method method,
                                                 http::uri uri,
                                                 hls_context *cxt) const noexcept
{
    auto path_items = uri.get_path_items();

    switch (method) {
    case http::request_line_method::post: {
        if (path_items.size() == 1) {
            if (path_items.at(0).compare("files") == 0) {
                cxt->method = hls_method::post_chunk;
                return {0, http::handle_res_type::success};
            }
        }
        break;
    }

    case http::request_line_method::get: {
        if (path_items.size() < 4 || path_items.size() > 7) {
            break;
        }

        if (path_items[0].compare("hls") != 0) {
            return {0, http::handle_res_type::error};
        }
        cxt->hls_id = std::string(path_items[1].data(), path_items[1].size());

        if (path_items[2].compare("live") == 0) {
            http::string file = path_items[3];
            http::string name = file.cut_by('.');
            if (file.compare("ts") == 0) {
                bool ok = false;
                cxt->seq = name.to_int(ok);
                if (ok) {
                    cxt->method = hls_method::get_chunk;
                    return {0, http::handle_res_type::success};
                }
            } else if (file.compare("m3u8") == 0) {
                cxt->method = hls_method::get_playlist;
                return {0, http::handle_res_type::success};
            }
        } else if (path_items[2].compare("archive")) {

        }

        break;
    }

    default:
        return {404, http::handle_res_type::error};
    }

    return {404, http::handle_res_type::error};
}

http::handle_res api::fetch_hls_context_from_header(http::request_line_method method,
                                                    http::string key,
                                                    http::string value,
                                                    hls_context *cxt) const noexcept
{
    if (method != http::request_line_method::post) {
        return {0, http::handle_res_type::ignore};
    }

    if (cxt->hls_id.empty() && key.compare("X-HLS-ID") == 0) {
        cxt->hls_id = std::string(value.data(), value.size());
        return {0,http::handle_res_type::success};
    } else if (cxt->start_unix_timestamp == -1 && key.compare("X-HLS-TIMESTAMP") == 0) {
        bool ok = false;
        cxt->start_unix_timestamp = value.to_int(ok);
        if (ok) {
            return {0, http::handle_res_type::success};
        } else {
            return {404, http::handle_res_type::error};
        }
    } else if (cxt->duration_msecs == -1 && key.compare("X-HLS-DURATION") == 0) {
        bool ok = false;
        cxt->duration_msecs = value.to_int(ok);
        if (ok) {
            return {0, http::handle_res_type::success};
        } else {
            return {404, http::handle_res_type::error};
        }
    } else if (cxt->seq == -1 && key.compare("X-HLS-SEQ") == 0) {
        bool ok = false;
        cxt->seq = value.to_int(ok);
        if (ok) {
            return {0, http::handle_res_type::success};
        } else {
            return {404, http::handle_res_type::error};
        }
    }

    return {0, http::handle_res_type::ignore};
}
