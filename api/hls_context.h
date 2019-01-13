#ifndef HLS_CONTEXT_H
#define HLS_CONTEXT_H

#include <string>

enum class hls_method
{
    undefined,
    post_chunk,
    get_live_chunk,
    get_live_playlist,
    get_archive_chunk,
    get_archive_playlist
};

struct hls_context
{
    std::string hls_id;
    int64_t seq = -1;
    int64_t start_ut_msecs = -1;
    int64_t duration_msecs = -1;

    std::string path;

    hls_method method = hls_method::undefined;
};

#endif // HLS_CONTEXT_H
