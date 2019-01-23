#include "hls_live_storage.h"

#include <sstream>

#include <glog/logging.h>

#include "../utility/datetime.h"

using namespace hls_live;

storage::storage(size_t live_size,
                 size_t keep_size,
                 const std::string& hostname) noexcept :
    _live_size(live_size),
    _keep_size(keep_size),
    _hostname(hostname)
{
}

std::tuple<int64_t, error_type>
storage::get_last_read(const std::string& plst_id) const noexcept
{
    auto plst = find_playlist(plst_id);
    if (plst != nullptr) {
        return {plst->last_read, error_type::no_error};
    }
    return {0, error_type::playlist_not_found};
}

error_type
storage::add_chunk(const std::string& plst_id,
                   const std::shared_ptr<chunk>& cnk) noexcept
{
    LOG(INFO) << "start live post_chunk " << plst_id << " "
              << cnk->seq << " "
              << cnk->start_ut_msecs << " "
              << cnk->duration_msecs;

    if (cnk->seq < 0
            || cnk->start_ut_msecs < 0
            || cnk->duration_msecs <= 0 ) {
        return error_type::invalid_in_paramets;
    }

    auto plst = find_or_create_playlist(plst_id);
    if (plst != nullptr) {
        if (_playlists.count(plst_id) <= 0) {
            std::unique_lock plst_lock(_plst_mtx);
            _playlists.insert({plst_id, plst});
            plst_lock.unlock();
        }
    } else {
        return error_type::internal_error;
    }

    plst->last_modyfied = datetime::unix_timestamp();

    std::unique_lock lock(plst->mtx);

    if (plst->chunks.empty()) {
        plst->chunks.push_back(cnk);
    } else if (cnk->seq == 0) {
        LOG(WARNING) << "clear all because seq is 0";
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

    plst->cache_txt = build_playlist_txt(plst);

    LOG(INFO) << "stop live post_chunk " << plst_id << " "
              << cnk->seq << " "
              << cnk->start_ut_msecs << " "
              << cnk->duration_msecs;

    return error_type::no_error;
}

std::tuple<std::shared_ptr<chunk>, error_type>
storage::get_chunk(const std::string& plst_id, int64_t seq) const noexcept
{
    LOG(INFO) << "start live get_chunk " << plst_id << " " << seq;

    auto plst = find_playlist(plst_id);
    if (plst == nullptr) {
        return {nullptr, error_type::playlist_not_found};
    }
    plst->last_read.store(datetime::unix_timestamp());

    std::shared_lock lock(plst->mtx);

    if (plst->chunks.empty()) {
        LOG(WARNING) << "playlist exists and is empty";
        return {nullptr, error_type::chunk_not_found};
    }

    auto&& front = plst->chunks.front();
    auto&& back = plst->chunks.back();
    if (seq >= front->seq && seq <= back->seq) {
        int64_t front_gap = seq - front->seq;
        LOG(INFO) << "stop live get_chunk " << plst_id << " " << seq;
        return {*(plst->chunks.cbegin() + front_gap), error_type::no_error};
    }

    return {nullptr, error_type::chunk_not_found};
}

std::tuple<std::string, error_type>
storage::get_playlist_txt(const std::string &plst_id) const noexcept
{
    LOG(INFO) << "start live get_playlist " << plst_id;

    auto plst = find_playlist(plst_id);
    if (plst == nullptr) {
        return {std::string(), error_type::playlist_not_found};
    }
    plst->last_read.store(datetime::unix_timestamp());

    std::shared_lock lock(plst->mtx);

    LOG(INFO) << "stop live get_playlist " << plst_id;

    return {plst->cache_txt, error_type::no_error};
}

std::tuple<int, error_type>
storage::delete_playlists(int64_t unmodified_secs) noexcept
{
    LOG(INFO) << "start delete live playlists " << unmodified_secs;

    int res = 0;

    std::unique_lock plst_lock(_plst_mtx);

    const int64_t now = datetime::unix_timestamp();

    auto i = _playlists.begin();
    while (i != _playlists.end()) {
        if (now - i->second->last_modyfied >= unmodified_secs) {
            const std::string id = i->first;

            LOG(INFO) << "start delete playlist " << id;
            i = _playlists.erase(i);
            res++;
            LOG(INFO) << "stop delete playlist " << id;
        } else {
            ++i;
        }
    }

    LOG(INFO) << "stop delete live playlists " << unmodified_secs;

    return {res, error_type::no_error};
}

std::shared_ptr<storage::playlist>
storage::find_playlist(const std::string& plst_id) const noexcept
{
    auto searched = _playlists.find(plst_id);
    if (searched == _playlists.cend()) {
        return nullptr;
    }
    return searched->second;
}

std::shared_ptr<storage::playlist>
storage::find_or_create_playlist(const std::string& plst_id) const noexcept
{
    auto plst = find_playlist(plst_id);
    if (plst == nullptr) {
        return std::make_shared<playlist>();
    }
    return plst;
}

std::string
storage::build_playlist_txt(std::shared_ptr<playlist>& plst) const noexcept
{
    if (plst->chunks.empty()) {
        LOG(WARNING) << "chunks empty while building playlist";
        return std::string();
    }

    auto beg = plst->chunks.cbegin();
    auto end = plst->chunks.cend();
    if (plst->chunks.size() > _live_size) {
        beg = end - static_cast<int64_t>(_live_size);
    }

    while (beg < end && (*beg)->buff == nullptr) {
        ++beg;
    }
    while ((end-1) > beg && (*(end-1))->buff == nullptr) {
        --end;
    }

    if (plst->chunks.empty()) {
        LOG(WARNING) << "chunks empty after trimming while building playlist";
        return std::string();
    }

    int64_t max_duration = 0;
    for (auto i = beg; i<plst->chunks.cend(); ++i) {
        const auto& cnk = (*i);
        if (cnk->duration_msecs > max_duration) {
            max_duration = cnk->duration_msecs;
        }
    }

    std::stringstream ss;
    ss << "#EXTM3U" << std::endl;
    ss << "#EXT-X-TARGETDURATION:" << max_duration/1000.0 << std::endl;
    ss << "#EXT-X-VERSION:3" << std::endl;
    ss << "#EXT-X-MEDIA-SEQUENCE:" << (*beg)->seq << std::endl;

    bool disc_added = false;
    for (auto i = beg; i<plst->chunks.cend(); ++i) {
        const auto& cnk = (*i);
        if (cnk->buff != nullptr) {
            disc_added = false;

            ss << "#EXTINF:" << cnk->duration_msecs/1000.0 << "," << std::endl;
            ss << build_chunk_url(cnk) << std::endl;
        } else if (!disc_added) {
            disc_added = true;
            ss << "#EXT-X-DISCONTINUITY";
        }
    }

    return ss.str();
}

std::string
storage::build_chunk_url(const std::shared_ptr<chunk>& cnk) const noexcept
{
    return std::string(std::to_string(cnk->seq) + ".ts");
}
