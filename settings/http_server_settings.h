#ifndef HTTP_SERVER_SETTINGS_H
#define HTTP_SERVER_SETTINGS_H

#include <string>

struct http_server_settings
{
    std::string host;
    unsigned int port = 0;
};

#endif // HTTP_SERVER_SETTINGS_H
