#ifndef HLS_LIVE_STORAGE_H
#define HLS_LIVE_STORAGE_H

#include <unordered_map>
#include <deque>
#include <memory>
#include <atomic>
#include <shared_mutex>

#include "../hls/chunk.h"
#include "hls_live_storage_error.h"

namespace hls_live {

class storage
{
public:
    storage(size_t live_size,
            size_t keep_size,
            const std::string& hostname) noexcept;

    std::tuple<int64_t, error_type>
    get_last_read(const std::string& plst_id) const noexcept;

    error_type
    add_chunk(const std::string& plst_id,
              const std::shared_ptr<chunk>& cnk) noexcept;

    std::tuple<std::shared_ptr<chunk>, error_type>
    get_chunk(const std::string& plst_id, int64_t seq) const noexcept;

    std::tuple<std::string, error_type>
    get_playlist_txt(const std::string& plst_id) const noexcept;

    std::tuple<int, error_type>
    delete_playlists(int64_t unmodified_secs) noexcept;

private:
    struct playlist
    {
        std::string cache_txt;
        std::atomic<int64_t> last_read = 0;
        std::atomic<int64_t> last_modyfied = 0;
        std::deque<std::shared_ptr<chunk>> chunks;
        mutable std::shared_mutex mtx;
    };

    inline std::shared_ptr<playlist>
    find_playlist(const std::string& plst_id) const noexcept;

    inline std::shared_ptr<playlist>
    find_or_create_playlist(const std::string& plst_id) const noexcept;

    inline std::string
    build_playlist(const std::string& plst_id,
                   std::shared_ptr<playlist>& plst) const noexcept;

    inline std::string
    build_chunk_url(const std::shared_ptr<chunk>& cnk) const noexcept;

private:
    const size_t _live_size = 0;
    const size_t _keep_size = 0;

    const std::string _hostname;

    mutable std::shared_mutex _plst_mtx;
    std::unordered_map<std::string, std::shared_ptr<playlist>> _playlists;
};

}

#endif // HLS_LIVE_STORAGE_H
