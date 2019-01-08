#include "string.h"

using namespace http;

http::string::string()
{
}

http::string::string(const char *buff, size_t size) :
    _buff(buff),
    _size(size)
{
}

bool http::string::empty() const
{
    return _size == 0;
}

size_t http::string::size() const
{
    return _size;
}

const char *http::string::data() const
{
    return _buff;
}

ssize_t http::string::find(char ch) const
{
    for(size_t i=0; i<_size; ++i) {
        if (_buff[i] == ch) {
            return static_cast<ssize_t>(i);
        }
    }
    return -1;
}

void http::string::trim()
{
    while(_size > 0 && _buff[0] == 0x20) {
        ++_buff;
        --_size;
    }

    while(_size > 0 && _buff[_size - 1] == 0x20) {
        --_size;
    }
}

http::string http::string::sub_to(char ch) const
{
    if (empty()) {
        return string();
    }

    if (_buff[0] == ch) {
        return string();
    }

    ssize_t find_res = find(ch);
    if (find_res != -1) {
        size_t pos = static_cast<size_t>(find_res);
        return string(_buff, pos);
    } else {
        return string();
    }
}

http::string http::string::cut_by(char ch)
{
    string res = sub_to(ch);
    if (!res.empty()) {
        // plus 1 ignores ch
        _size -= res.size() + 1;
        _buff += res.size() + 1;
    } else if (_size > 0 && _buff[0] == ch) {
        --_size;
        ++_buff;
    }
    return res;
}

int64_t http::string::to_int(bool &ok) const
{
    int64_t res = 0;

    if (empty()) {
        ok = false;
        return res;
    }

    ok = true;
    for (size_t i=0; i<_size; ++i) {
        if (_buff[i] >= 0x30 && _buff[i] <= 0x39) {
            res = res*10 + _buff[i] - '0';
        } else {
            ok = false;
            break;
        }
    }

    return res;
}
