#ifndef ARCHIVE_HLS_SETTINGS_H
#define ARCHIVE_HLS_SETTINGS_H

#include <string>
#include <vector>

struct archive_dummy_segment
{
    std::string path;
    int durration;
};

struct archive_hls_settings
{
    std::string archive_dir_path;
    std::string mongo_uri;
    std::vector<archive_dummy_segment> dummy_segments;
};

#endif // ARCHIVE_HLS_SETTINGS_H
