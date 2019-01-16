#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>

class filesystem
{
public:
    static ssize_t file_size(int fd);
    static bool dir_is_exist(const std::string& path);
    static bool create_directory(const std::string& path);
    static bool create_file(const std::string& path, char* buff, size_t size);
};

#endif // FILESYSTEM_H
