#ifndef CHUNK_H
#define CHUNK_H

#include <string>

struct chunk
{
    ~chunk()
    {
        if (buff != nullptr) {
            delete[] buff;
        }
    }

    char* buff = nullptr;
    size_t size = 0;
    int64_t seq = -1;
    int64_t start_ut_msecs = 0;
    int64_t duration_msecs = 0;
};

#endif // CHUNK_H
