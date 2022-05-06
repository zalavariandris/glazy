#pragma once

#include "glad/glad.h"
#include <vector>
#include <tuple>
#include <string>
#include <OpenEXR/ImfPixelType.h>

struct PBOImage {
    GLuint id;
    std::vector<std::string> channels;
    std::tuple<int, int, int, int> bbox;
    Imf::PixelType pixeltype;
};

class PBOImageStream {
public:
    std::vector<PBOImage> pbos;
    int display_index;
    int write_index;
    int m_width, m_height;

    void* ptr;

public:
    PBOImageStream(int width, int height, int channels, int n);

    void onGUI();

    void reformat(int width, int height, int channels, int n);

    ~PBOImageStream();

    // display PBO bbox
    std::tuple<int, int, int, int> bbox() const;


    void write(void* pixels, const std::tuple<int, int, int, int>& bbox, const std::vector<std::string>& channels, unsigned long long typesize);

    void begin();

    void end(std::tuple<int, int, int, int> bbox, std::vector<std::string> channels);
};