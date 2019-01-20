#ifndef HLS_LIVE_STORAGE_ERROR_H
#define HLS_LIVE_STORAGE_ERROR_H

namespace hls_live {

enum class error_type
{
    no_error,
    invalid_in_paramets,
    playlist_not_found,
    chunk_not_found,
    internal_error
};

}

#endif // HLS_LIVE_STORAGE_ERROR_H
