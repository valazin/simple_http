#ifndef HLS_ARHIVE_STORAGE_H
#define HLS_ARHIVE_STORAGE_H

#include <string>
#include <memory>
#include <vector>

#include "../hls/chunk.h"
#include "hls_chunk_info.h"

class hls_chunk_info_repository;
class hls_arhive_playlist_generator;

class hls_archive_storage
{
public:
    hls_archive_storage(const std::string& dir_path,
                       const std::string& hostname,
                       const std::string& mongo_uri,
                       const std::vector<hls_chunk_info>& dummy_list);

    bool add_chunk(const std::string& hls_id, const std::shared_ptr<chunk>& cnk) noexcept;
    std::string get_playlist(const std::string& hls_id, int64_t start_ut_msecs, int64_t duration_msecs) const noexcept;

private:

private:
    std::string _arhive_dir_path;
    std::string _host_name;
    std::shared_ptr<hls_chunk_info_repository> _info_repository;
    std::shared_ptr<hls_arhive_playlist_generator> _playlist_generator;
};

#endif // HLS_ARHIVE_STORAGE_H
