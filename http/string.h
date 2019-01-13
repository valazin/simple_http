#ifndef STRING_H
#define STRING_H

#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>

namespace http
{

struct string
{
    string();
    string(const char* buff, size_t size);

    bool empty() const;
    size_t size() const;
    const char* data() const;

    ssize_t find(char ch) const;
    int compare(const char *str) const;

    void trim();
    string sub_to(char ch) const;
    string cut_by(char ch);
    std::vector<string> split(char ch) const;

    int64_t to_int(bool& ok) const;
    std::string to_str() const;

private:
    const char* _buff = nullptr;
    size_t _size = 0;
};

}

#endif // STRING_H
