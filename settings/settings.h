#ifndef SETTINGS_H
#define SETTINGS_H

#include <tuple>
#include <libconfig.h++>

#include "http_server_settings.h"
#include "live_hls_settings.h"
#include "archive_hls_settings.h"

class settings
{
public:
    settings(const std::string& file_path);

    std::tuple<http_server_settings, bool> get_http_server_settings() const;
    std::tuple<live_hls_settings, bool> get_live_hls_settings() const;
    std::tuple<archive_hls_settings, bool> get_archive_hls_settings() const;

private:
    libconfig::Config _config;
};

#endif // SETTINGS_H
