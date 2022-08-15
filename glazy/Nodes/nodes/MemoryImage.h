#pragma once

#include <vector>
#include <string>

#include <OpenImageIO/typedesc.h>
#include <glad/glad.h>

class MemoryImage
{
public:
    void* data = nullptr;
    int width;
    int height;
    std::vector<std::string> channels;
    OIIO::TypeDesc format;

    // Allocate rquired memory
    MemoryImage(int width, int height, std::vector<std::string> channels, OIIO::TypeDesc format);

    // This will take ownership of the data pinter. therefore it frees the memory address when destructed!
    MemoryImage(void* data, int width, int height, std::vector<std::string> channels, OIIO::TypeDesc format) :
        data(data),
        width(width),
        height(height),
        channels(channels),
        format(format)
    {}

    void realloc();

    /// copy constructor
    MemoryImage(const MemoryImage& other);

    /// size in bytes
    size_t bytes() const;

    GLenum glformat() const;

    GLenum gltype() const;

    ~MemoryImage();
};