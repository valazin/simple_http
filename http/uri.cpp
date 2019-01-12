#include "uri.h"

#include <cassert>

using namespace http;

uri::uri() noexcept
{
}

// FIXME: /path?sta?rt=10&&&: onle one ?
// FIXME: path///?start=10&&&duration=5: must starts with /
uri::uri(char* buff, size_t size) noexcept
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
