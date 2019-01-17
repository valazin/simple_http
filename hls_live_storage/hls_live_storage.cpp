#include "hls_live_storage.h"

#include <deque>
#include <sstream>
#include <shared_mutex>
#include <algorithm>

#include <glog/logging.h>

struct playlist
{
    std::string cache_txt;
    std::deque<std::shared_ptr<chunk>> chunks;
    mutable std::shared_mutex mtx;
};

hls_live_storage::hls_live_storage(size_t live_size,
                                   size_t keep_size,
                                   const std::string &hostname) noexcept :
    _live_size(live_size),
    _keep_size(keep_size),
    _hostname(hostname)
{
}

bool hls_live_storage::add_chunk(const std::string &plst_id, const std::shared_ptr<chunk> &cnk) noexcept
{
    LOG(INFO) << "start live post_chunk " << plst_id << " "
              << cnk->seq << " "
              << cnk->start_ut_msecs << " "
              << cnk->duration_msecs;

    if (cnk->seq < 0
            || cnk->start_ut_msecs < 0
            || cnk->duration_msecs <= 0 ) {
        return false;
    }

    // TODO: app map mutex. not important
    playlist* plst = find_or_create_playlist(plst_id);
    if (plst != nullptr) {
        _playlists.insert({plst_id, plst});
    } else {
        return false;
    }

    std::unique_lock lock(plst->mtx);

    if (plst->chunks.empty()) {
        plst->chunks.push_back(cnk);
    } else if (cnk->seq == 0) {
        LOG(INFO) << "WARNING: clear all because seq 0";
        plst->chunks.clear();
        plst->chunks.push_back(cnk);
    } else {
        auto last_cnk = plst->chunks.back();
        int64_t back_gap = cnk->seq - last_cnk->seq;
        if (back_gap < 0) {
            // put before end
            auto first_cnk = plst->chunks.front();
            int64_t front_gap = first_cnk->seq - cnk->seq;
            if (front_gap > 0) {
                // put before first
                int64_t avaible = static_cast<int64_t>(_keep_size - plst->chunks.size());
                if (avaible > front_gap) {
                    // fill gap
                    for (int64_t i=front_gap-1; i>0; --i) {
                        auto dummy = std::make_shared<chunk>();
                        dummy->seq = cnk->seq + i;
                        plst->chunks.push_front(dummy);
                    }
                    plst->chunks.push_front(cnk);
                }
            } else {
                // put into the middle
                auto i = plst->chunks.end() - (-1*back_gap) - 1;
                (*i) = cnk;
            }
        } else if (back_gap == 0) {
            // ignore
            LOG(WARNING) << "ignore add becuase seq already exists";
        } else if (back_gap >= 1) {
            // put after end
            if (static_cast<size_t>(back_gap) <= _live_size) {
                // fill gap
                for (int64_t i=0; i<back_gap-1; ++i) {
                    auto dummy = std::make_shared<chunk>();
                    dummy->seq = (last_cnk->seq + 1) + i;
                    plst->chunks.push_back(dummy);
                }
            } else {
                LOG(WARNING) << "clear all because too big seq";
                plst->chunks.clear();
            }

            plst->chunks.push_back(cnk);
        }

        int64_t diff = static_cast<int64_t>(plst->chunks.size() - _keep_size);
        if (diff > 0) {
            plst->chunks.erase(plst->chunks.cbegin(), plst->chunks.cbegin() + diff);
        }
    }

    plst->cache_txt = build_playlist(plst_id, plst);

    LOG(INFO) << "stop live post_chunk " << plst_id << " "
              << cnk->seq << " "
              << cnk->start_ut_msecs << " "
              << cnk->duration_msecs;

    return true;
}

std::shared_ptr<chunk> hls_live_storage::get_chunk(const std::string &plst_id, int64_t seq) const noexcept
{
    LOG(INFO) << "start live get_chunk " << plst_id << " " << seq;

    // TODO: app map mutex. not important
    playlist* plst = find_playlist(plst_id);
    if (plst == nullptr) {
        return nullptr;
    }

    std::shared_lock lock(plst->mtx);

    auto&& front = plst->chunks.front();
    auto&& back = plst->chunks.back();
    if (seq >= front->seq && seq <= back->seq) {
        int64_t front_gap = seq - plst->chunks.front()->seq;
        LOG(INFO) << "stop live get_chunk " << plst_id << " " << seq;
        return *(plst->chunks.cbegin() + front_gap);
    }

    LOG(INFO) << "fail stop live get_chunk " << plst_id << " " << seq;
    return nullptr;
}

std::string hls_live_storage::get_playlist(const std::string &plst_id) const noexcept
{
    LOG(INFO) << "start live get_playlist " << plst_id;

    // TODO: app map mutex. not important
    playlist* plst = find_playlist(plst_id);
    if (plst == nullptr) {
        return std::string();
    }

    std::shared_lock lock(plst->mtx);
    LOG(INFO) << "stop live get_playlist " << plst_id;
    return plst->cache_txt;
}

playlist* hls_live_storage::find_playlist(const std::string &plst_id) const noexcept
{
    auto searched = _playlists.find(plst_id);
    if (searched == _playlists.end()) {
        return nullptr;
    }
    return searched->second;
}

playlist *hls_live_storage::find_or_create_playlist(const std::string &plst_id) const noexcept
{
    playlist* plst = find_playlist(plst_id);
    if (plst == nullptr) {
        plst = new playlist;
    }
    return plst;
}

std::string hls_live_storage::build_playlist(const std::string &plst_id, playlist *plst) const noexcept
{
    if (plst->chunks.empty()) {
        return std::string();
    }

    auto beg = plst->chunks.cbegin();
    if (plst->chunks.size() > _live_size) {
        beg = plst->chunks.cend() - static_cast<int64_t>(_live_size);
    }

    int64_t max_duration = 0;
    for (auto i = beg; i<plst->chunks.cend(); ++i) {
        const auto cnk = (*i);
        if (cnk->duration_msecs > max_duration) {
            max_duration = cnk->duration_msecs;
        }
    }

    std::stringstream ss;
    ss << "#EXTM3U" << std::endl;
    ss << "#EXT-X-TARGETDURATION:" << max_duration/1000.0 << std::endl;
    ss << "#EXT-X-VERSION:3" << std::endl;
    ss << "#EXT-X-MEDIA-SEQUENCE:" << (*beg)->seq << std::endl;

    for (auto i = beg; i<plst->chunks.cend(); ++i) {
        const auto cnk = (*i);
        ss << "#EXTINF:" << cnk->duration_msecs/1000.0  << "," << std::endl;
        ss << build_chunk_url(plst_id, cnk) << std::endl;
    }

    return ss.str();
}

std::string hls_live_storage::build_chunk_url(const std::string &plst_id, const std::shared_ptr<chunk> &cnk) const noexcept
{
    return std::string("http://" + _hostname + "/hls/" + plst_id + "/live/" + std::to_string(cnk->seq) + ".ts");
}
