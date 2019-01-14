#include "api/api.h"
#include "hls_live_storage/hls_live_storage.h"
#include "hls_archive_storage/hls_arhive_storage.h"

// TODO: one thread eat 100% cpu after client stop loading
// TODO: error when requesting from curl
// TODO: handling "client close connection" event
// TODO: last read
// TODO: clear by timer
// TODO: define max size for every state in http. safe from buffer overflow
// TODO: for resp buff we must make a smart pointer: because chunk may be deleted while we are sending it through socket
// TODO: how to return string without copy
// TODO: can we read until EAGAIN

#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

int main()
{
//    const std::string host = "10.110.3.43";
//    const uint16_t port = 1024;
//    const std::string hostname = host + ":" + std::to_string(port);

//    const size_t live_size = 5;
//    const size_t keep_size = 20;
//    hls_live_storage* live_storage = new hls_live_storage(live_size, keep_size, hostname);

//    const std::string arhive_dir_path = "/tmp/hls";
//    const std::string mongo_uri;
//    std::vector<hls_chunk_info> dummy_list;
//    hls_archive_storage* archive_storage = new hls_archive_storage(arhive_dir_path, hostname, mongo_uri, dummy_list);

//    api a(live_storage, nullptr);
//    a.start(host, port);

    int in_fd = open("hello",
                     O_RDONLY,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);;
    if (in_fd == -1) {
        perror("open in file");
        return -1;
    }

    int out_fd = open("hello_copy",
                     O_WRONLY | O_CREAT | O_TRUNC,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (out_fd == -1) {
        perror("open out file");
        return -1;
    }

    off_t offset = 0;
    ssize_t sended_size = sendfile(out_fd, in_fd, &offset, 40);
    if (sended_size == -1) {
        perror("sendfile");
    }
    std::cout << offset << " " << sended_size;

    close(in_fd);
    close(out_fd);

    return  0;
}
