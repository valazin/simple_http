#ifndef CHUNK_INFO_SERIALIZER_H
#define CHUNK_INFO_SERIALIZER_H

#include <bsoncxx/document/view_or_value.hpp>

#include "hls_chunk_info.h"

class chunk_info_serializer
{
public:
    static bsoncxx::document::value serialize(const hls_chunk_info& info);
    static hls_chunk_info deserialize(const bsoncxx::document::view& document);
};

#endif // CHUNK_INFO_SERIALIZER_H
