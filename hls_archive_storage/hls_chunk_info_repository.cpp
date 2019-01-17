#include "hls_chunk_info_repository.h"

#include <glog/logging.h>

#include <mongocxx/uri.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/server_error_code.hpp>
#include <mongocxx/exception/operation_exception.hpp>

#include "chunk_info_serializer.h"

using namespace bsoncxx::builder::basic;

hls_chunk_info_repository::hls_chunk_info_repository(const std::string &mongo_uri)
{
    try {
        mongocxx::instance::current();
        auto uri = mongocxx::uri{mongo_uri};
        _pool = std::make_unique<mongocxx::pool>(uri);
    }
    catch (const mongocxx::v_noabi::operation_exception& e) {
        LOG(ERROR) << "DB connection error" << e.what();
    }
}

bool hls_chunk_info_repository::add(const hls_chunk_info &info)
{
    LOG(INFO) << "start add info"
              << info.hls_id << " "
              << info.start_ut_msecs  << " "
              << info.duration_msecs;

    mongocxx::database db;
    try {
        auto client = _pool->acquire();
        db = (*client)[_db_name];
        if (!db.has_collection(info.hls_id)) {
            db.create_collection(info.hls_id);
        }
    } catch (const mongocxx::operation_exception &exception) {
        LOG(ERROR) << "Collection create error" << exception.what() << " " << exception.code();
        // TODO: 48 has already created?
        if (exception.code().value() == 48) {
        } else {
            return false;
        }
    } catch (const std::exception &exception) {
        LOG(ERROR) << "Collection create error" << exception.what();
        return false;
    }

    try {
        mongocxx::collection collection = db[info.hls_id];

        auto doc = chunk_info_serializer::serialize(info);
        collection.insert_one(std::move(doc));

        LOG(INFO) << "stop add "
                  << info.hls_id << " "
                  << info.start_ut_msecs  << " "
                  << info.duration_msecs;

        return true;
    } catch (const std::exception &exception) {
        LOG(ERROR) << "Insert segment info error" << exception.what();
        return false;
    }
}

std::vector<hls_chunk_info> hls_chunk_info_repository::get_list(const std::string& hls_id,
                                                                int64_t start_ut_msecs,
                                                                int64_t duration_msecs) const
{
    LOG(INFO) << "start get playlist "
              << hls_id << " "
              << start_ut_msecs << " "
              << duration_msecs;

    std::vector<hls_chunk_info> result;
    try {
        auto client = _pool->acquire();
        mongocxx::database db = (*client)[_db_name];

        if (!db.has_collection(hls_id)) {
            return result;
        }
        mongocxx::collection collection = db[hls_id];

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
        mongocxx::cursor cursor = collection.aggregate(pipeline);
        for(const bsoncxx::document::view& doc : cursor) {
            hls_chunk_info info = chunk_info_serializer::deserialize(doc);
            result.push_back(info);
        }
    } catch (const std::exception &exception) {
        LOG(ERROR) << "Find collection error " << exception.what();
        return result;
    }

    LOG(INFO) << "stop get playlist "
              << hls_id << " "
              << start_ut_msecs << " "
              << duration_msecs;

    return result;
}
