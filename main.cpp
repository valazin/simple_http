#include "api/api.h"
#include "hls_live_storage/hls_live_storage.h"

// TODO: one thread eat 100% cpu after client stop loading

int main()
{
    const std::string host = "10.110.3.43";
    const uint16_t port = 1024;
    const std::string hostname = host + ":" + std::to_string(port);

    const size_t live_size = 5;
    const size_t keep_size = 10;
    hls_live_storage* live_storage = new hls_live_storage(live_size, keep_size, hostname);

    api a(live_storage);
    a.start(host, port);

    return  0;
}
