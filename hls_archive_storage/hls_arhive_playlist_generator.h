#ifndef HLS_ARHIVE_PLAYLIST_GENERATOR_H
#define HLS_ARHIVE_PLAYLIST_GENERATOR_H

#include <memory>
#include <sstream>

#include "hls_chunk_info_repository.h"

class hls_chunk_info_repository;

class hls_arhive_playlist_generator
{
public:
    hls_arhive_playlist_generator(const std::vector<hls_chunk_info>& dummy_list,
                                  std::shared_ptr<hls_chunk_info_repository> info_repository);

    std::string generate(const std::string &hls_id,
                         int64_t start_ut_msecs,
                         int64_t duration_msecs, const std::string &base_uri) const noexcept;

private:
    void append_header(std::stringstream& stream) const noexcept;
    void append_chunk(int64_t msecs, const std::string& uri, std::stringstream& stream) const noexcept;
    void append_discontinuity_chunk(int64_t msecs, const std::string& uri, std::stringstream& stream) const noexcept;
    void append_dummy_chunk(int64_t msecs, std::stringstream &stream, const std::string &base_uri) const noexcept;

private:
    std::vector<hls_chunk_info> _dummy_list;
    std::shared_ptr<hls_chunk_info_repository> _info_repository;
};

#endif // HLS_ARHIVE_PLAYLIST_GENERATOR_H
