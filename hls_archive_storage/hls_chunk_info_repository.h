#ifndef CHUNK_INFO_REPOSITORY_H
#define CHUNK_INFO_REPOSITORY_H

#include <string>
#include <vector>
#include <memory>
#include <mongocxx/pool.hpp>

#include "hls_chunk_info.h"

class hls_chunk_info_repository
{
public:
    hls_chunk_info_repository(const std::string& mongo_uri);

    bool add(const hls_chunk_info &info);
    std::vector<hls_chunk_info> get_list(const std::string& hls_id,
                                         int64_t start_ut_msecs,
                                         int64_t duration_msecs) const;

private:
    std::unique_ptr<mongocxx::pool> _pool;
    const std::string _db_name = "HlsStorage";
};

#endif // CHUNK_INFO_REPOSITORY_H
