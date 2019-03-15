#ifndef CONTENT_TYPES_H
#define CONTENT_TYPES_H

namespace http {

enum class content_types
{
    none,
    text,
    json,
    hls_playlist,
    hls_chunk
};

}

#endif // CONTENT_TYPES_H
