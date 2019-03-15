#include "buffer.h"

using namespace http;

buffer::buffer(char *buff, size_t size) noexcept :
    _buff(buff),
    _buff_size(size)
{
}

buffer::~buffer()
{
    if (_buff != nullptr && _buff_size > 0) {
        delete[] _buff;
    }
}

size_t buffer::size() const noexcept
{
    return _buff_size;
}

const char *buffer::data() const noexcept
{
    return _buff;
}
