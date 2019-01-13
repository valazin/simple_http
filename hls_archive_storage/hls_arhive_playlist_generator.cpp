#include "hls_arhive_playlist_generator.h"

#include <cassert>
#include <iostream>

#include "hls_chunk_info_repository.h"

hls_arhive_playlist_generator::hls_arhive_playlist_generator(const std::string &host_name,
                                                             const std::vector<hls_chunk_info> &dummy_list,
                                                             std::shared_ptr<hls_chunk_info_repository> info_repository) :
    _host_name(host_name),
    _dummy_list(dummy_list),
    _info_repository(info_repository)
{
}

std::string hls_arhive_playlist_generator::generate(const std::string &hls_id,
                                                    int64_t
                                                    start_ut_msecs,
                                                    int64_t duration_msecs) const noexcept
{
    std::stringstream stream;

    append_header(stream);

    std::vector<hls_chunk_info> list = _info_repository->get_list(hls_id, start_ut_msecs, duration_msecs);
    if (list.empty()) {
        std::cout << "Not found chunk for " << hls_id << " startTime " << start_ut_msecs << " duration " << duration_msecs;
        std::cout << "Fill playlist with dummy chunks";
        append_dummy_chunk(duration_msecs, stream);
        return stream.str();
    }

    const hls_chunk_info& front = list.front();
    const long front_gap = front.start_ut_msecs - start_ut_msecs;
    if (front_gap > 0) {
        std::cout << "Front gap " << front_gap << " " << hls_id << " startTime " << start_ut_msecs << " duration " << duration_msecs;
        append_dummy_chunk(front_gap, stream);
    }

    for (size_t i=0; i<list.size(); ++i) {
        const hls_chunk_info& current_info = list.at(i);
        if (i > 0) {
            const hls_chunk_info& prev_info = list.at(i-1);
            long gapMsecs = current_info.start_ut_msecs - (prev_info.start_ut_msecs + prev_info.duration_msecs);
            if (gapMsecs > 0) {
                append_dummy_chunk(gapMsecs, stream);
            }
        }

        const long duration = current_info.duration_msecs;
        const std::string uri = _host_name + current_info.path;

        append_chunk(duration, uri, stream);
    }

        const hls_chunk_info& back = list.back();
        const long back_gap = (start_ut_msecs+duration_msecs) - (back.start_ut_msecs+back.duration_msecs);
        if (back_gap > 0) {
            std::cout << "Back gap " << back_gap << " " << hls_id << " startTime " << start_ut_msecs << " duration " << duration_msecs;
            append_dummy_chunk(back_gap, stream);
        }

    stream << "#EXT-X-ENDLIST";

    return stream.str();
}

void hls_arhive_playlist_generator::append_header(std::stringstream& stream) const noexcept
{
    stream << "#EXTM3U" << std::endl;
    stream << "#EXT-X-VERSION:3" << std::endl;
    // TODO: define max duraion in one place for stream muxer and stream generator
    stream << "#EXT-X-TARGETDURATION:11" << std:: endl;
    stream << "#EXT-X-MEDIA-SEQUENCE:0" << std::endl;
}

void hls_arhive_playlist_generator::append_chunk(int64_t msecs,
                                                 const std::string& uri,
                                                 std::stringstream& stream) const noexcept
{
    // TODO: use form live storage method
    stream << "#EXTINF:" << msecs/1000.0 << "," << std::endl;
    stream << uri << std::endl;
}

void hls_arhive_playlist_generator::append_discontinuity_chunk(int64_t msecs,
                                                               const std::string& uri,
                                                               std::stringstream& stream) const noexcept
{
    stream << "#EXT-X-DISCONTINUITY" << std::endl;
    append_chunk(msecs, uri, stream);
}

void hls_arhive_playlist_generator::append_dummy_chunk(int64_t msecs,
                                                       std::stringstream& stream) const noexcept
{
    if (msecs <= 0) {
        return;
    }

    for (auto&& dummy_info : _dummy_list) {
        if (msecs <= 0) {
            break;
        }

        const int64_t duration_msecs = dummy_info.duration_msecs;
        const std::string uri = _host_name + dummy_info.path;
        const int64_t count = msecs / duration_msecs;

        for (unsigned int i=0; i<count; ++i) {
            append_discontinuity_chunk(dummy_info.duration_msecs, uri, stream);
            msecs -= duration_msecs;
        }
    }

    if (msecs > 0) {
        auto&& smallest_chunk = _dummy_list.back();

        assert(smallest_chunk.duration_msecs > msecs);

        const std::string uri = _host_name + smallest_chunk.path;
        append_discontinuity_chunk(msecs, uri, stream);

        msecs -= msecs;
    }

    stream << "#EXT-X-DISCONTINUITY" << std::endl;

    assert(msecs == 0);
}
