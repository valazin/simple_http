#include <thread>
#include <glog/logging.h>

#include "settings/settings.h"
#include "api/api.h"
#include "hls_live_storage/hls_live_storage.h"
#include "hls_archive_storage/hls_arhive_storage.h"

// bugs:
// TODO: use shared buff. We don't know this buff is needed to delete or free
// TODO: safe path to storage
// TODO: for every hls a separate collection in mongo
// TODO: sendfile is can block by disk
// TODO: define max size for every state in http to safe from buffer overflow

// feature:
// TODO: rest error code
// TODO: clear by timer
// TODO: config
// TODO: logs
// TODO: docker
// TODO: archive timeline
// TODO: archive size
// TODO: delete archive

// optimization:
// TODO: can we read/write until EAGAIN
// TODO: for every request create buffer for escape copy
// TODO: test without mutex. if it will give advantageous use stick by hls id

// refactoring:
// TODO: cmake
// TODO: make router
// TODO: use location option for storage module
// TODO: shared pointer cbuff: chunk may be deleted while we are sending it through socket

int main()
{
    settings conf("/opt/conf");

    auto [http_conf, http_ok] = conf.get_http_server_settings();
    if (!http_ok) {
        return -1;
    }
    const std::string host = http_conf.host;
    const unsigned int port = http_conf.port;
    const std::string hostname = host + ":" + std::to_string(port);

    auto [live_conf, live_ok] = conf.get_live_hls_settings();
    hls_live::storage* live_storage = nullptr;
    if (live_ok) {
        live_storage = new hls_live::storage(live_conf.live_size,
                                             live_conf.keep_size,
                                             hostname);
    }

    auto [archive_conf, archive_ok] = conf.get_archive_hls_settings();
    hls_archive_storage* archive_storage = nullptr;
    if (archive_ok) {
        std::vector<hls_chunk_info> dummy_list;
        for (auto dummy : archive_conf.dummy_segments) {
            hls_chunk_info info;
            info.path = dummy.path;
            info.duration_msecs = dummy.durration;
            dummy_list.push_back(info);
        }

        archive_storage = new hls_archive_storage(archive_conf.archive_dir_path,
                                                  hostname,
                                                  archive_conf.mongo_uri,
                                                  dummy_list);
    }

    if (!live_storage && !archive_storage) {
        LOG(ERROR) << "couldn't create any valid storage";
        return -1;
    }

    api a(live_storage, archive_storage);
    a.start(host, port);

    if (live_storage) {
        // use main thread like auxilary worker
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            live_storage->delete_playlists(30);
        }
    }
}
