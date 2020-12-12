#ifndef STRING_H
#define STRING_H

#include <vector>
#include <string>

namespace http {

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

    // TODO: refactoring: use template to_int() instead of this
    int64_t to_int(bool& ok) const;
    std::string to_str() const;

    template<class T>
    std::tuple<T, bool> to_int() const
    {
        T res = 0;

        if (empty()) {
            return {res, false};
        }

        bool ok = true;
        for (size_t i=0; i<_size; ++i) {
            if (_buff[i] >= 0x30 && _buff[i] <= 0x39) {
                res = res*10 + _buff[i] - '0';
            } else {
                ok = false;
                break;
            }
        }

        return {res, ok};
    }

private:
    const char* _buff = nullptr;
    size_t _size = 0;
};

}

#endif // STRING_H
