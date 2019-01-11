#include "hls_live_storage.h"

#include <deque>
#include <sstream>
#include <iostream>
#include <shared_mutex>
#include <algorithm>

struct playlist
{
    std::string cache_txt;
    std::deque<std::shared_ptr<chunk>> chunks;
    mutable std::shared_mutex mtx;
};

hls_live_storage::hls_live_storage(size_t live_size,
                                   size_t keep_size,
                                   const std::string &hostname) :
    _live_size(live_size),
    _keep_size(keep_size),
    _hostname(hostname)
{
}

bool hls_live_storage::add_chunk(const std::string &plst_id, const std::shared_ptr<chunk> &cnk)
{
    std::cout << "post_chunk " << plst_id << " "
              << cnk->seq << " "
              << cnk->start_unix_timestamp << " "
              << cnk->duration_msecs
              << std::endl << std::flush;

    if (cnk->seq == -1
            || cnk->start_unix_timestamp == -1
            || cnk->duration_msecs == -1 ) {
        return false;
    }

    playlist* plst = find_playlist(plst_id);
    if (plst == nullptr) {
        plst = new playlist;
        if (!plst) {
            std::cerr << "couldn't allocate memory for playlist" << std::endl;
            return false;
        }
        _playlists.insert({plst_id, plst});
    }

    std::unique_lock lock(plst->mtx);

    // if cnk->seq = 0 then clear playlist

    if (plst->chunks.empty()) {
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
                std::cout << "WARNING: clear all: is coming big seq" << std::endl << std::flush;
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

    return true;
}

std::shared_ptr<chunk> hls_live_storage::get_chunk(const std::string &plst_id, int64_t seq) const noexcept
{
    std::cout << "get_chunk " << plst_id << " " << seq << std::endl << std::flush;

    playlist* plst = find_playlist(plst_id);
    if (plst == nullptr) {
        return nullptr;
    }

    std::shared_lock lock(plst->mtx);

    auto i = std::find_if(plst->chunks.begin(), plst->chunks.end(), [seq](auto&& cnk) {
        return cnk->seq == seq;
    });

    if (i != plst->chunks.end()) {
        return (*i);
    }

    return nullptr;
}

std::string hls_live_storage::get_playlist(const std::string &plst_id) const noexcept
{
    std::cout << "get_playlist " << plst_id << std::endl << std::flush;

    playlist* plst = find_playlist(plst_id);
    if (plst == nullptr) {
        return std::string();
    }

    std::shared_lock lock(plst->mtx);
    return plst->cache_txt;
}

playlist* hls_live_storage::find_playlist(const std::string &plst_id) const noexcept
{
    // todo: app map mutex
    auto searched = _playlists.find(plst_id);
    if (searched == _playlists.end()) {
        return nullptr;
    }
    return searched->second;
}

std::string hls_live_storage::build_playlist(const std::string &plst_id, playlist *plst) noexcept
{
    if (plst->chunks.empty()) {
        return std::string();
    }

    auto beg = plst->chunks.begin();
    auto end = plst->chunks.end();
    if (plst->chunks.size() > _live_size) {
        beg = end - static_cast<int64_t>(_live_size);
    }

    // TODO: calculate TARGETDURATION
    // TODO: calculate EXTINF

    std::stringstream ss;
    ss << "#EXTM3U" << std::endl;
    ss << "#EXT-X-TARGETDURATION:2" << std::endl;
    ss << "#EXT-X-VERSION:3" << std::endl;
    ss << "#EXT-X-MEDIA-SEQUENCE:" << (*beg)->seq << std::endl;
    while (beg != plst->chunks.end()) {
        auto cnk = (*beg);
        ss << "#EXTINF:" << "2.000000," << std::endl;
        ss << build_chunk_url(plst_id, cnk) << std::endl;
        ++beg;
    }

    return ss.str();
}

std::string hls_live_storage::build_chunk_url(const std::string &plst_id, const std::shared_ptr<chunk> &cnk) const noexcept
{
    return std::string("http://" + _hostname + "/hls/" + plst_id + "/" + std::to_string(cnk->seq) + ".ts");
}
