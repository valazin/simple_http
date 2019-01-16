#include "chunk_info_serializer.h"

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/stdx/string_view.hpp>

bsoncxx::document::value chunk_info_serializer::serialize(const hls_chunk_info &info)
{
    auto builder = bsoncxx::builder::stream::document{};
    bsoncxx::document::value result = builder
                                     << "StreamId" << info.hls_id
                                     << "StartUnixTimestamp" << info.start_ut_msecs
                                     << "EndUnixTimestamp" << info.start_ut_msecs + info.duration_msecs
                                     << "VideoFilePath" << info.path
                                     << "SizeInBytes" << static_cast<int64_t>(info.size)
                                     << bsoncxx::builder::stream::finalize;
    return result;
}

hls_chunk_info chunk_info_serializer::deserialize(const bsoncxx::document::view &document)
{
    int64_t segmentStartTime = 0;
    if (document.find("StartUnixTimestamp") != document.cend()) {
        segmentStartTime = document["StartUnixTimestamp"].get_int64();
    }
    int64_t segmentEndTime = 0;
    if (document.find("EndUnixTimestamp") != document.cend()) {
        segmentEndTime = document["EndUnixTimestamp"].get_int64();
    }
    std::string segmentVideoFilePath;
    if (document.find("VideoFilePath") != document.cend()) {
        bsoncxx::stdx::string_view videoFilePathView = document["VideoFilePath"].get_utf8().value;
        segmentVideoFilePath = videoFilePathView.to_string();
    }
    std::string segmentStreamId;
    if (document.find("StreamId") != document.cend()) {
        bsoncxx::stdx::string_view streamIdView = document["StreamId"].get_utf8().value;
        segmentStreamId = streamIdView.to_string();
    }
    int64_t segmentSizeInBytes = 0;
    if (document.find("SizeInBytes") != document.cend()) {
        segmentSizeInBytes = document["SizeInBytes"].get_int64();
    }

    hls_chunk_info result;
    result.hls_id = segmentStreamId;
    result.path = segmentVideoFilePath;
    result.start_ut_msecs = segmentStartTime;
    result.duration_msecs = segmentEndTime - segmentStartTime;
    result.size = static_cast<size_t>(segmentSizeInBytes);

    return result;
}
