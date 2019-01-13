#include "hls_chunk_info_repository.h"

hls_chunk_info_repository::hls_chunk_info_repository(const std::string &mongo_uri)
{
    // TODO: throw exception if connect to mongo was failed
}

bool hls_chunk_info_repository::add(const hls_chunk_info &info) noexcept
{
    return false;
}

std::vector<hls_chunk_info> hls_chunk_info_repository::get_list(const std::string& hls_id,
                                                                int64_t start_ut_msecs,
                                                                int64_t duration_msecs) const noexcept
{
}
