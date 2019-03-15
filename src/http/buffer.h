#ifndef BUFFER_H
#define BUFFER_H

#include <string>

namespace http {

class buffer
{
public:
    buffer(char* buff, size_t size) noexcept;

    ~buffer();

    size_t size() const noexcept;
    const char* data() const noexcept;

private:
    std::string _str;

    char* _buff = nullptr;
    size_t _buff_size = 0;
};

}

#endif // BUFFER_H
