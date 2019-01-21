#include "settings.h"

#include <glog/logging.h>

using namespace libconfig;

settings::settings(const std::string &file_path)
{
    _config.readFile(file_path.c_str());
}

std::tuple<http_server_settings, bool> settings::get_http_server_settings() const
{
    http_server_settings result;

    const Setting& root = _config.getRoot();
    try {
        const Setting& server = root["http_server"];
        server.lookupValue("host", result.host);
        server.lookupValue("port", result.port);
    } catch (const SettingNotFoundException& exception) {
        LOG(ERROR) << "get_http_server_settings '" << exception.what();
        return {result, false};
    }

    return {result, true};
}

std::tuple<live_hls_settings, bool> settings::get_live_hls_settings() const
{
    live_hls_settings result;

    const Setting& root = _config.getRoot();
    try {
        const Setting& live_hls = root["live_hls"];
        live_hls.lookupValue("live_size", result.live_size);
        live_hls.lookupValue("keep_size", result.keep_size);
    } catch (const SettingNotFoundException& exception) {
        LOG(ERROR) << "get_live_hls_settings '" << exception.what();
        return {result, false};
    }

    return {result, true};
}

std::tuple<archive_hls_settings, bool> settings::get_archive_hls_settings() const
{
    archive_hls_settings result;

    const Setting& root = _config.getRoot();
    try {
        const Setting& archive_hls = root["archive_hls"];
        archive_hls.lookupValue("archive_dir_path", result.archive_dir_path);
        archive_hls.lookupValue("mongo_uri", result.mongo_uri);

        const Setting& segments = archive_hls["dummy_segments"];

        const int count = segments.getLength();
        if (count > 0) {
            result.dummy_segments.reserve(static_cast<size_t>(count));
        }

        for (int i=0; i<count; ++i) {
            const Setting& segment_setting = segments[i];

            archive_dummy_segment segment;
            segment_setting.lookupValue("path", segment.path);
            segment_setting.lookupValue("duration", segment.durration);

            result.dummy_segments.push_back(segment);
        }
    } catch (const SettingNotFoundException& exception) {
        LOG(ERROR) << "get_live_hls_settings '" << exception.what();
        return {result, false};
    }

    return {result, true};
}
