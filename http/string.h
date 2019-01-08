#include <cstdio>

namespace http
{

struct string
{
    string();
    string(char* buff, size_t size);

    bool empty() const;
    size_t size() const;
    const char* data() const;

    ssize_t find(char ch) const;

    void trim();
    string sub_to(char ch) const;
    string cut_by(char ch);

    int64_t to_int(bool& ok) const;

private:
    char* _buff = nullptr;
    size_t _size = 0;
};

}

