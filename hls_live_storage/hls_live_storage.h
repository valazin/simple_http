#ifndef HLS_LIVE_STORAGE_H
#define HLS_LIVE_STORAGE_H

#include <map>
#include <memory>

#include "../hls/chunk.h"

struct playlist;

class hls_live_storage
{
public:
    hls_live_storage(size_t live_size, size_t keep_size, const std::string& hostname) noexcept;

    bool add_chunk(const std::string& plst_id, const std::shared_ptr<chunk>& cnk) noexcept;
    std::shared_ptr<chunk> get_chunk(const std::string& plst_id, int64_t seq) const noexcept;
    std::string get_playlist(const std::string& plst_id) const noexcept;

private:
    inline playlist* find_playlist(const std::string& plst_id) const noexcept;
    inline playlist* find_or_create_playlist(const std::string& plst_id) const noexcept;

    inline std::string build_playlist(const std::string& plst_id, playlist* plst) const noexcept;
    inline std::string build_chunk_url(const std::string& plst_id, const std::shared_ptr<chunk>& cnk) const noexcept;
    inline static std::string build_chunk_duration(int64_t duration_msecs) noexcept;

private:
    const size_t _live_size = 0;
    const size_t _keep_size = 0;
    const std::string _hostname;
    std::map<std::string, playlist*> _playlists;
};

#endif // HLS_LIVE_STORAGE_H
