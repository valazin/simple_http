#ifndef HLS_CHUNK_INFO_H
#define HLS_CHUNK_INFO_H

#include <string>

struct hls_chunk_info
{
    std::string hls_id;
    int64_t seq = -1;
    int64_t start_ut_msecs = 0;
    int64_t duration_msecs = 0;
    std::string path;
};

#endif // HLS_CHUNK_INFO_H
