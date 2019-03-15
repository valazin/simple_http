#include <iostream>

#include "http/server.h"

struct context
{
    int id;
};

int main()
{
    int id_counter = 0;
    auto uri_handler = [&id_counter](
            std::shared_ptr<http::request> req,
            http::uri uri) -> int {

        std::cout << "handling uri " << uri.to_str() << std::endl << std::flush;

        auto cxt = std::make_shared<context>();
        cxt->id = id_counter++;

        req->user_data = cxt;

        return 0;
    };

    auto header_handler = [](std::shared_ptr<http::request> req,
            http::string key,
            http::string value) -> int {

        auto cxt = std::reinterpret_pointer_cast<context>(req->user_data);
        if (!cxt) {
            return 500;
        }

        std::cout << "handling header " << key.to_str() << ":" <<  value.to_str()
                   << " "  << cxt->id << std::endl << std::flush;

        if (value.compare("bad_value") == 0) {
            // server will response 403
            return 400;
        } else if (value.compare("ignore_me")) {
            // server will save the header into response::headers
            return -1;
        } else {
            // server will not save the header
            return 0;
        }
    };

    auto req_handler = [](std::shared_ptr<http::request> req) -> http::response {
        http::response resp;

        auto cxt = std::reinterpret_pointer_cast<context>(req->user_data);
        if (!cxt) {
            resp.code = 500;
            return resp;
        }

        std::cout << "handling request " << cxt->id << std::endl << std::flush;

        if (req->method == http::request_method::post) {
            std::string data(req->body_buff->data(), req->body_buff->size());
            if (data == "hello") {
                resp.code = 200;
            } else {
                resp.code = 401;
            }
        } else {
            resp.code = 404;
        }

        return resp;
    };

    http::server server;
    if (server.start("127.0.0.1", 1025, req_handler, uri_handler, header_handler)) {
        std::cout << "listen localhost:1025" << std::endl << std::flush;
    } else {
        std::cerr << "coulnd't start server" << std::endl << std::flush;
        return -1;
    }

    while (true) {
        // do useful work
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    return  0;
}
