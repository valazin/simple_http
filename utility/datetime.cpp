#include "datetime.h"

#include <ctime>

int64_t datetime::unix_timestamp()
{
    return std::time(nullptr);
}
