#include "api/api.h"
#include "hls_live_storage/hls_live_storage.h"
#include "hls_archive_storage/hls_arhive_storage.h"

// bugs:
// TODO: for every hls a separate collection in mongo
// TODO: safe path to storage
// TODO: sendfile is can block by disk
// TODO: handler when close connnection
// TODO: htop says that only two thread works
// TODO: one thread eat 100% cpu after client stop loading
// TODO: error when requesting from curl
// TODO: handling "client close connection" event
// TODO: define max size for every state in http to safe from buffer overflow
// TODO: shared pointer cbuff: chunk may be deleted while we are sending it through socket

// optimization:
// TODO: can we read/write until EAGAIN
// TODO: for every request create buffer for escape copy
// TODO: test without mutex. if it will give advantageous use stick by hls id

// feature:
// TODO: clear by timer
// TODO: archive timeline
// TODO: delete archive
// TODO: rest error code
// TODO: config
// TODO: logs
// TODO: docker

// refactoring:
// TODO: make router
// TODO: use location option for storage module

int main()
{
    const std::string host = "10.110.3.43";
    const uint16_t port = 1030;
    const std::string hostname = host + ":" + std::to_string(port);

    const size_t live_size = 5;
    const size_t keep_size = 20;
    hls_live_storage* live_storage = new hls_live_storage(live_size, keep_size, hostname);

    const std::string arhive_dir_path = "/home/valeriy/Faceter/hls_archive";
    const std::string mongo_uri = "mongodb://10.110.3.43:27017";

    std::vector<hls_chunk_info> dummy_list;

    hls_chunk_info dummy_10sec;
    dummy_10sec.path = "/tmp/hls/dummy_10s.ts";
    dummy_10sec.duration_msecs = 9960;
    dummy_list.push_back(dummy_10sec);

    hls_chunk_info dummy_1sec;
    dummy_1sec.path = "/tmp/hls/dummy_1s.ts";
    dummy_1sec.duration_msecs = 9960;
    dummy_list.push_back(dummy_1sec);

    hls_archive_storage* archive_storage = new hls_archive_storage(arhive_dir_path, hostname, mongo_uri, dummy_list);

    api a(live_storage, archive_storage);
    a.start(host, port);
}
