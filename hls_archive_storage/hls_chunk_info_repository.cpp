#include "hls_chunk_info_repository.h"

#include <glog/logging.h>

#include <mongocxx/uri.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/exception/server_error_code.hpp>
#include <mongocxx/exception/operation_exception.hpp>

#include "chunk_info_serializer.h"

using namespace bsoncxx::builder::basic;

hls_chunk_info_repository::hls_chunk_info_repository(const std::string &mongo_uri)
{
//    // TODO: throw exception if connect to mongo was failed
    try {
        mongocxx::instance::current();
        auto mongoUri = mongocxx::uri{mongo_uri};
        _dbClient = mongocxx::client(mongoUri);
        _streamsRecordsDatabase = _dbClient.database("HlsStorage");
        _isConnectedToDatabase = true;
    }
    catch (const mongocxx::v_noabi::operation_exception& e) {
        LOG(ERROR) << "DB connection error" << e.what();
        _isConnectedToDatabase = false;
    }
}

bool hls_chunk_info_repository::add(const hls_chunk_info &info) noexcept
{
    if (!_streamsRecordsDatabase.has_collection(info.hls_id)) {
        try {
            _streamsRecordsDatabase.create_collection(info.hls_id);
        }
        catch (const mongocxx::operation_exception &exception) {
            LOG(ERROR) << "Collection create error" << exception.what() << " " << exception.code();
            // TODO: 48 has already created?
            if (exception.code().value() == 48) {
            } else {
                return false;
            }
        }
        catch (const mongocxx::exception &exception) {
            LOG(ERROR) << "Collection create error" << exception.what();
            return false;
        }
    }

    mongocxx::collection streamCollection = _streamsRecordsDatabase[info.hls_id];

    auto segmentInfoDoc = chunk_info_serializer::serialize(info);

    try {
        streamCollection.insert_one(std::move(segmentInfoDoc));
    }
    catch (const mongocxx::exception &exception) {
        LOG(ERROR) << "Insert segment info error" << exception.what();
    }
}

std::vector<hls_chunk_info> hls_chunk_info_repository::get_list(const std::string& hls_id,
                                                                int64_t start_ut_msecs,
                                                                int64_t duration_msecs) const
{
    std::vector<hls_chunk_info> result;

    if (!_streamsRecordsDatabase.has_collection(hls_id)) {
        return result;
    }

    try {
        mongocxx::collection streamCollection = _streamsRecordsDatabase[hls_id];

        mongocxx::pipeline pipeline{};
        pipeline.match(make_document(
                           kvp("StartUnixTimestamp", make_document(
                                   kvp("$lte", start_ut_msecs + duration_msecs)
                                   )),
                           kvp("EndUnixTimestamp", make_document(
                                   kvp("$gte", start_ut_msecs)
                                   ))
                           ));
        pipeline.sort(make_document(kvp("StartUnixTimestamp", 1)));
        mongocxx::cursor cursor = streamCollection.aggregate(pipeline);
        for(const bsoncxx::document::view& doc : cursor) {
            hls_chunk_info info = chunk_info_serializer::deserialize(doc);
            result.push_back(info);
        }
    }
    catch (const mongocxx::exception &exception) {
        LOG(ERROR) << "Find collection error " << exception.what();
    }

    return result;
}
