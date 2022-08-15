#include "MemoryImage.h"

MemoryImage::MemoryImage(int width, int height, std::vector<std::string> channels, OIIO::TypeDesc format) :
    width(width),
    height(height),
    channels(channels),
    format(format)
{
    // allocate Image
    std::cout << "allocate image: " << width << "x" << height << " #" << channels.size() << "(" << format.c_str() << ")" << "\n";
    data = (void*)std::malloc(bytes());
    if (data == nullptr) {
        throw std::bad_alloc();
    }
}

void MemoryImage::realloc()
{
    data = std::realloc(data, bytes());
}

MemoryImage::MemoryImage(const MemoryImage& other)
{
    // free current memory if allocated
    if (data != nullptr) free(data);

    // allocate memory
    data = (void*)malloc(other.bytes());

    // copy pixel data
    std::memcpy(data, other.data, other.bytes());

    // copy info
    width = other.width;
    height = other.height;
    channels = other.channels;
    format = other.format;
}

size_t MemoryImage::bytes() const
{
    return width * height * channels.size() * format.size();
}

GLenum MemoryImage::glformat() const
{
    GLenum glformat;
    if (channels == std::vector<std::string>({ "B", "G", "R", "A" }))
    {
        return GL_BGRA;
    }
    else if (channels == std::vector<std::string>({ "B", "G", "R" }))
    {
        return GL_BGR;
    }
    else if (channels.size() == 4)
    {
        return GL_RGBA;
    }
    else if (channels.size() == 3)
    {
        return GL_RGB;
    }
}

GLenum MemoryImage::gltype() const
{
    if (format == OIIO::TypeHalf) return GL_HALF_FLOAT;
    else if (format == OIIO::TypeUInt8) return GL_UNSIGNED_BYTE;
    else throw "format is not recognized";
}

MemoryImage::~MemoryImage()
{
    std::cout << "free MemoryImage" << "\n";
    if (data != nullptr) free(data);
}