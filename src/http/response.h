#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>
#include <memory>

#include "buffer.h"
#include "content_types.h"

namespace http {

struct response
{
    int code = 0;
    content_types content_type = content_types::none;

    //
    std::string body_str;
    //
    std::shared_ptr<buffer> body_buff;
    //
    std::string body_file_path;
};

}

#endif // RESPONSE_H
