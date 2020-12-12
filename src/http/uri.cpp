#include "uri.h"

#include <cassert>

using namespace http;

uri::uri() noexcept
{
}

// TODO: bug: /path?sta?rt=10&&&: onle one ?
// TODO: bug: path///?start=10&&&duration=5: must starts with /
uri::uri(const char* buff, size_t size) noexcept :
    _buff(buff),
    _size(size)
{
    _is_valid = false;

    string str(buff, size);
    str.trim();

    string path_str = str.cut_by('?');
    string query_str;
    if (path_str.empty()) {
        if (!str.empty()) {
            path_str = str;
            query_str = string();
        } else {
            return;
        }
    } else if (!str.empty()) {
        query_str = str;
    }

    assert(!path_str.empty());

    _path_items = path_str.split('/');
    if (_path_items.empty()) {
        return;
    }

    if (!query_str.empty()) {
        std::vector<string> quaries = query_str.split('&');
        if (quaries.empty()) {
            quaries.push_back(query_str);
        }

        assert(!quaries.empty());

        for (string query : quaries) {
            string key = query.cut_by('=');
            if (!key.empty() && !query.empty()) {
                _query_items.push_back({key, query});
            } else {
                return;
            }
        }
    }

    _is_valid = true;
}

bool uri::is_valid() const
{
    return _is_valid;
}

std::vector<string> uri::get_path_items() const noexcept
{
    return _path_items;
}

std::vector<query> uri::get_query_items() const noexcept
{
    return _query_items;
}

string uri::find_query_item(const char* key) const noexcept
{
    for (const auto& item : _query_items) {
        if (item.first.compare(key) == 0) {
            return item.second;
        }
    }
    return string();
}

std::string uri::to_str() const noexcept
{
    if (_is_valid) {
        return std::string(_buff, _size);
    } else {
        return std::string();
    }
}
