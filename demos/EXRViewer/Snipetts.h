#pragma once

/// Read pixels to memory
void exr_to_memory(Imf::InputPart& inputpart, int x, int y, int width, int height, std::vector<std::string> channels, void* memory)
{
    //ZoneScoped;
    Imf::FrameBuffer frameBuffer;
    //size_t chanoffset = 0;
    unsigned long long xstride = sizeof(half) * channels.size();
    char* buf = (char*)memory;
    buf -= (x * xstride + y * width * xstride);

    size_t chanoffset = 0;
    for (auto name : channels)
    {
        size_t chanbytes = sizeof(half);
        frameBuffer.insert(name,   // name
            Imf::Slice(Imf::PixelType::HALF, // type
                buf + chanoffset,           // base
                xstride,
                (size_t)width * xstride
            )
        );
        chanoffset += chanbytes;
    }

    inputpart.setFrameBuffer(frameBuffer);
    inputpart.readPixels(y, y + height - 1);
};

void get_bbox(Imf::InputPart& inputpart, std::tuple<int, int, int, int>* bbox)
{
    /// Update datawindow
    {
        //ZoneScopedN("Update datawindow");
        Imath::Box2i dataWindow = inputpart.header().dataWindow();
        *bbox = std::tuple<int, int, int, int>(dataWindow.min.x, dataWindow.min.y, dataWindow.max.x - dataWindow.min.x + 1, dataWindow.max.y - dataWindow.min.y + 1);
    }
}

/// bbox: x,y,width, height
/// <param name="pixels"></param>
///
/// 

void exr_to_memory(std::filesystem::path filename, int part, std::vector<std::string> channels, std::tuple<int, int, int, int>* bbox, void* memory) {
    /// Open Current InputPart
    std::unique_ptr<Imf::MultiPartInputFile> inputfile;
    std::unique_ptr<Imf::InputPart> inputpart;
    {
        //ZoneScopedN("Open Curren InputPart");

        try
        {
            //ZoneScopedN("Open Current File");
            inputfile = std::make_unique<Imf::MultiPartInputFile>(filename.string().c_str());

        }
        catch (const Iex::InputExc& ex) {
            std::cerr << "file doesn't appear to really be an EXR file" << "\n";
            std::cerr << "  " << ex.what() << "\n";
            return;
        }

        {
            //ZoneScopedN("seek part");
            inputpart = std::make_unique<Imf::InputPart>(*inputfile, part);
        }
    }

    get_bbox(*inputpart, bbox);
    auto [x, y, w, h] = *bbox;
    exr_to_memory(*inputpart, x, y, w, h, channels, memory);
}

void memory_to_texture(void* memory, int x, int y, int width, int height, std::vector<std::string> channels, GLenum data_gltype, GLuint output_tex)
{
    std::array<GLint, 4> swizzle_mask{ GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    auto data_glformat = glformat_from_channels(channels, swizzle_mask);
    glInvalidateTexImage(output_tex, 1);
    glBindTexture(GL_TEXTURE_2D, output_tex);
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, data_glformat, data_gltype, memory);
    glBindTexture(GL_TEXTURE_2D, 0);
};

void memory_to_pbo() {

}

void pbo_to_tex()
{

}