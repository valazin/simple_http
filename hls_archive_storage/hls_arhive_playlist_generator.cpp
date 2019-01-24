#include "hls_arhive_playlist_generator.h"

#include <cassert>
#include <glog/logging.h>

#include "hls_chunk_info_repository.h"

hls_arhive_playlist_generator::hls_arhive_playlist_generator(const std::vector<hls_chunk_info> &dummy_list,
                                                             std::shared_ptr<hls_chunk_info_repository> info_repository) :
    _dummy_list(dummy_list),
    _info_repository(info_repository)
{
}

std::string hls_arhive_playlist_generator::generate(const std::string &hls_id,
                                                    int64_t
                                                    start_ut_msecs,
                                                    int64_t duration_msecs,
                                                    const std::string &base_uri) const noexcept
{
    std::stringstream stream;

    append_header(stream);

    std::vector<hls_chunk_info> list = _info_repository->get_list(hls_id, start_ut_msecs, duration_msecs);
    if (list.empty()) {
        LOG(INFO) << "Not found chunk for " << hls_id << " startTime " << start_ut_msecs << " duration " << duration_msecs;
        LOG(INFO) << "Fill playlist with dummy chunks";
        append_dummy_chunk(duration_msecs, stream, base_uri);
        return stream.str();
    }

    // fill front gap
    const hls_chunk_info& front = list.front();
    const long front_gap = front.start_ut_msecs - start_ut_msecs;
    if (front_gap > 0) {
        LOG(INFO) << "Front gap " << front_gap << " " << hls_id << " startTime " << start_ut_msecs << " duration " << duration_msecs;
        append_dummy_chunk(front_gap, stream, base_uri);
    }

    for (size_t i=0; i<list.size(); ++i) {
        const hls_chunk_info& current = list.at(i);
        if (i > 0) {
            const hls_chunk_info& prev = list.at(i-1);
            long gap = current.start_ut_msecs - (prev.start_ut_msecs + prev.duration_msecs);
            if (gap > 0) {
                append_dummy_chunk(gap, stream, base_uri);
            }
        }

        const long duration = current.duration_msecs;
        const std::string uri = base_uri + current.path;

        append_chunk(duration, uri, stream);
    }

    // fill back gap
    const hls_chunk_info& back = list.back();
    const long back_gap = (start_ut_msecs+duration_msecs) - (back.start_ut_msecs+back.duration_msecs);
    if (back_gap > 0) {
        LOG(INFO) << "Back gap " << back_gap << " " << hls_id << " startTime " << start_ut_msecs << " duration " << duration_msecs;
        append_dummy_chunk(back_gap, stream, base_uri);
    }

    stream << "#EXT-X-ENDLIST";

    return stream.str();
}

void hls_arhive_playlist_generator::append_header(std::stringstream& stream) const noexcept
{
    stream << "#EXTM3U" << std::endl;
    stream << "#EXT-X-VERSION:3" << std::endl;
    // TODO: define max duraion
    stream << "#EXT-X-TARGETDURATION:11" << std:: endl;
    stream << "#EXT-X-MEDIA-SEQUENCE:0" << std::endl;
}

void hls_arhive_playlist_generator::append_chunk(int64_t msecs,
                                                 const std::string& uri,
                                                 std::stringstream& stream) const noexcept
{
    stream << "#EXTINF:" << msecs/1000.0 << "," << std::endl;
    // TODO: fix resolve uri
    stream << uri.substr(1, uri.size()-1) << std::endl;
}

void hls_arhive_playlist_generator::append_discontinuity_chunk(int64_t msecs,
                                                               const std::string& uri,
                                                               std::stringstream& stream) const noexcept
{
    stream << "#EXT-X-DISCONTINUITY" << std::endl;
    append_chunk(msecs, uri, stream);
}

void hls_arhive_playlist_generator::append_dummy_chunk(int64_t msecs,
                                                       std::stringstream& stream,
                                                       const std::string& base_uri) const noexcept
{
    if (msecs <= 0) {
        return;
    }

    for (auto&& dummy_info : _dummy_list) {
        if (msecs <= 0) {
            break;
        }

        const int64_t duration_msecs = dummy_info.duration_msecs;
        const std::string uri = base_uri + dummy_info.path;
        const int64_t count = msecs / duration_msecs;

        for (unsigned int i=0; i<count; ++i) {
            append_discontinuity_chunk(dummy_info.duration_msecs, uri, stream);
            msecs -= duration_msecs;
        }
    }

    if (msecs > 0) {
        auto&& smallest_chunk = _dummy_list.back();

        assert(smallest_chunk.duration_msecs > msecs);

        const std::string uri = base_uri + smallest_chunk.path;
        append_discontinuity_chunk(msecs, uri, stream);

        msecs -= msecs;
    }

    stream << "#EXT-X-DISCONTINUITY" << std::endl;

    assert(msecs == 0);
}
