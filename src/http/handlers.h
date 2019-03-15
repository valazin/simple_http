#ifndef HANDLERS_H
#define HANDLERS_H

#include <memory>
#include <functional>

#include "uri.h"
#include "request.h"
#include "response.h"

namespace http {

typedef std::function<response(std::shared_ptr<request>)> request_handler;
typedef std::function<void(std::shared_ptr<response>)> response_handler;
typedef std::function<int(std::shared_ptr<request>, uri)> uri_handler;
typedef std::function<int(std::shared_ptr<request>, string, string)> header_handler;

}

#endif // HANDLERS_H
