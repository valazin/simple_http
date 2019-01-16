#include "hls_arhive_storage.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <ctime>

#include <glog/logging.h>

#include "../utility/filesystem.h"
#include "hls_chunk_info_repository.h"
#include "hls_arhive_playlist_generator.h"

hls_archive_storage::hls_archive_storage(const std::string& dir_path,
                                       const std::string &hostname,
                                       const std::string &mongo_uri,
                                       const std::vector<hls_chunk_info>& dummy_list) :
    _arhive_dir_path(dir_path),
    _host_name(hostname)
{
    if (!filesystem::create_directory(dir_path)) {
        // TODO: throw except
    }

    _info_repository = std::make_shared<hls_chunk_info_repository>(mongo_uri);

    _playlist_generator = std::make_shared<hls_arhive_playlist_generator>(dummy_list, _info_repository);
}

bool hls_archive_storage::add_chunk(const std::string &hls_id, const std::shared_ptr<chunk> &cnk) noexcept
{
    std::time_t start_ut_secs = cnk->start_ut_msecs / 1000;
    struct std::tm* utc_date = gmtime(&start_ut_secs);

    int day = utc_date->tm_mday;
    int month = utc_date->tm_mon + 1;
    int year = utc_date->tm_year + 1900;

    // TODO: don't check with syscall, use map for that
    std::string dir_path = _arhive_dir_path + "/" + hls_id;
    if (!filesystem::dir_is_exist(dir_path) && !filesystem::create_directory(dir_path)) {
        return false;
    }

    dir_path +=  "/" + std::to_string(year);
    if (!filesystem::dir_is_exist(dir_path) && !filesystem::create_directory(dir_path)) {
        return false;
    }

    dir_path +=  "/" + std::to_string(month);
    if (!filesystem::dir_is_exist(dir_path) && !filesystem::create_directory(dir_path)) {
        return false;
    }

    dir_path +=  "/" + std::to_string(day);
    if (!filesystem::dir_is_exist(dir_path) && !filesystem::create_directory(dir_path)) {
        return false;
    }

    std::string file_path = dir_path + "/" + std::to_string(cnk->start_ut_msecs) + "-" + std::to_string(cnk->duration_msecs) + ".ts";

    int fd = open(file_path.data(),
                  O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (fd != -1) {
        ssize_t write_size = write(fd, cnk->buff, cnk->size);
        close(fd);
        if (write_size != -1) {
            if (static_cast<size_t>(write_size) < cnk->size) {
                LOG(WARNING) << "not all data written to file";
                // TODO: how to resolve this problem?
            }

             hls_chunk_info info;
            info.hls_id = hls_id;
            info.seq = cnk->seq;
            info.start_ut_msecs = cnk->start_ut_msecs;
            info.duration_msecs = cnk->duration_msecs;
            info.size = cnk->size;
            info.path = file_path;

            if (!_info_repository->add(info)) {
                // TODO
            }

            return true;
        } else {
            perror("write chunk");
        }
    } else {
        perror("open chunk for write");
    }

    return false;
}

std::string hls_archive_storage::get_chunk_path(const std::string &hls_id, const std::string &key) noexcept
{
    (void)hls_id;
    return "/" + key;
}

std::string hls_archive_storage::get_playlist(const std::string &hls_id,
                                             int64_t start_ut_msecs,
                                             int64_t duration_msecs) const noexcept
{
    return _playlist_generator->generate(hls_id, start_ut_msecs, duration_msecs, "http://" + _host_name + "/hls/" + hls_id + "/archive");
}
