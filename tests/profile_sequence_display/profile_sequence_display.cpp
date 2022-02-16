// profile_sequence_display.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "../../tracy/Tracy.hpp"

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"

// OpenImageIO
#include "OpenImageIO/imageio.h""
#include "OpenImageIO/imagecache.h"

// OpenEXR
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfStdIO.h>
#include <OpenEXR/ImfArray.h> //Imf::Array2D
#include <OpenEXR/half.h> // <half> type


#include <OpenEXR/ImfChannelList.h>

#include <imgui_internal.h>



/// Get opengl format for OIIO spec
/// internalformat, format, type</returns>
std::tuple<GLint, GLenum, GLenum> typespec_to_opengl(const OIIO::ImageSpec& spec, int nchannels)
{
    GLint glinternalformat;
    GLenum glformat;
    GLenum gltype;

    switch (spec.format.basetype) {
    case OIIO::TypeDesc::FLOAT: gltype = GL_FLOAT; break;
    case OIIO::TypeDesc::HALF: gltype = GL_HALF_FLOAT; break;
    case OIIO::TypeDesc::INT: gltype = GL_INT; break;
    case OIIO::TypeDesc::UINT: gltype = GL_UNSIGNED_INT; break;
    case OIIO::TypeDesc::INT16: gltype = GL_SHORT; break;
    case OIIO::TypeDesc::UINT16: gltype = GL_UNSIGNED_SHORT; break;
    case OIIO::TypeDesc::INT8: gltype = GL_BYTE; break;
    case OIIO::TypeDesc::UINT8: gltype = GL_UNSIGNED_BYTE; break;
    default:
        gltype = GL_UNSIGNED_BYTE;  // punt
        break;
    }

    bool issrgb = spec.get_string_attribute("oiio:ColorSpace") == "sRGB";

    glinternalformat = nchannels;
    if (nchannels == 1) {
        glformat = GL_LUMINANCE;
        if (spec.format.basetype == OIIO::TypeDesc::UINT8) {
            glinternalformat = GL_R8;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::UINT16) {
            glinternalformat = GL_R16F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::FLOAT)
        {
            glinternalformat = GL_R32F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::HALF) {
            glinternalformat = GL_R16F;
        }
    }
    else if (nchannels == 2) {
        glformat = GL_LUMINANCE_ALPHA;
        if (spec.format.basetype == OIIO::TypeDesc::UINT8) {

            glinternalformat = GL_RG8;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::UINT16) {
            glinternalformat = GL_RG16F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::FLOAT)
        {
            glinternalformat = GL_RG32F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::HALF) {
            glinternalformat = GL_RG16F;
        }
    }
    else if (nchannels == 3) {
        glformat = GL_RGB;
        if (spec.format.basetype == OIIO::TypeDesc::UINT8) {
            glinternalformat = GL_RGB8;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::UINT16) {
            glinternalformat = GL_RGB16;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::FLOAT) {
            glinternalformat = GL_RGB32F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::HALF) {
            glinternalformat = GL_RGB16F;
        }
    }
    else if (nchannels == 4) {
        glformat = GL_RGBA;
        if (spec.format.basetype == OIIO::TypeDesc::UINT8) {
            glinternalformat = GL_RGBA8;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::UINT16) {
            glinternalformat = GL_RGBA16;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::FLOAT) {
            glinternalformat = GL_RGBA32F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::HALF) {
            glinternalformat = GL_RGBA16F;
        }
    }
    else {
        glformat = GL_INVALID_ENUM;
        glinternalformat = GL_INVALID_ENUM;
    }

    return { glinternalformat, glformat, gltype };
}

//
// Return the size of a single value of the indicated type,
// in the machine's native format.
//
int pixelTypeSize(Imf::PixelType t)
{
    int size;

    switch (t)
    {
    case Imf::UINT:

        size = sizeof(unsigned int);
        break;

    case Imf::HALF:

        size = sizeof(half);
        break;

    case Imf::FLOAT:

        size =sizeof(float);
        break;

    default:
        
        throw IEX_NAMESPACE::ArgExc("Unknown pixel type.");
    }

    return size;
}

namespace WithImageInput {
    void ProfileSequenceDisplay()
    {
        OIIO::attribute("threads", 1);
        OIIO::attribute("exr_threads", 8);
        OIIO::attribute("try_all_readers", 0);
        OIIO::attribute("openexr:core", 0);

        auto [pattern, first_frame, last_frame, current_Frame] = scan_for_sequence("C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr");
        int F = first_frame;

        ZoneScoped;

        glazy::init();
        glfwSwapInterval(0);

        GLuint tex_roi = 0;
        glGenTextures(1, &tex_roi);
        glBindTexture(GL_TEXTURE_2D, tex_roi);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        GLuint pbo = 0;
        glGenBuffers(1, &pbo);

        auto start_time = std::chrono::steady_clock::now();
        TracyMessage("START", 5);
        while (glazy::is_running())
        {
            glazy::new_frame();
            // get frame filename
            std::filesystem::path filename;
            {
                ZoneScopedN("get sequence item");
                filename = sequence_item(pattern, F);
            }
            assert(("file does not exist", std::filesystem::exists(filename)));
            ImGui::Text("sequence: %d / %d-%d", F, first_frame, last_frame);
            ImGui::TextColored(std::filesystem::exists(filename) ? ImColor(1.0f, 1.0f, 1.0f) : ImColor(1.0f, 0.0f, 0.0f), "filename: %s", filename.string().c_str());

            // open image
            OIIO::ImageSpec spec;
            std::unique_ptr<OIIO::ImageInput> in;
            {
                ZoneScopedN("open image");
                in = OIIO::ImageInput::open(filename.string());
            }

            /// Make Texture from file
            {
                ZoneScopedN("make texture from file");

                // Read spec            
                {
                    ZoneScopedN("get spec");
                    spec = in->spec();
                    std::cout << filename << " | nchannels:" << spec.nchannels << "\n";
                }

                /// Allocate
                void* data;
                {
                    ZoneScopedN("Allocate");
                    auto type_size = spec.format.size();
                    data = malloc(spec.width * spec.height * 3 * type_size);
                }

                /// Get Pixels
                {
                    ZoneScopedN("Read pixels");
                    in->read_image(0, 3, spec.format, data);
                }

                // Upload to gpu
                {
                    ZoneScopedN("Upload image to GPU");
                    //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
                    //glBufferData(GL_PIXEL_UNPACK_BUFFER, spec.width*spec.height*3, data, GL_STREAM_DRAW);
                    glBindTexture(GL_TEXTURE_2D, tex_roi);

                    auto [internalformat, format, type] = typespec_to_opengl(spec, 3);
                    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, spec.width, spec.height, 0, format, type, data);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                }

                // Deallocate data after uploaded to GPU
                {
                    ZoneScopedN("Deallocate data");
                    free(data);
                }
            }

            /// Draw scene
            {
                ZoneScopedN("Draw Scene");
                imdraw::quad(tex_roi);
            }

            //if (ImGui::Begin("settings")) {
            //    static int threads{1};
            //    ImGui::DragInt("threads", &threads, 1, 8);
            //    static int exr_threads{ 1 };
            //    ImGui::DragInt("exr-threads", &exr_threads, 1, 8);
            //}
            //ImGui::End();

            glazy::end_frame();

            F++;
            if (F > last_frame)
            {
                ZoneScopedN("invalidate");
                F = first_frame;
                glazy::quit();

            }

            FrameMark;
        }
        TracyMessage("END", 3);
        auto end_time = std::chrono::steady_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto n_frames = last_frame - first_frame + 1;
        std::cout << "read: " << pattern.filename().string() << " " << n_frames << " frames in: " << dt / n_frames << " per frame\n";
        glazy::destroy();
    }
}


std::string to_string(Imf::PixelType t) {
    switch (t)
    {
    case Imf::UINT:
        return "Imf::UINT";
    case Imf::HALF:
        return "Imf::HALF";
    case Imf::FLOAT:
        return "Imf::FLOAT";
    default:
        return "[UNKNOWN]";
        break;
    }
}

std::string to_string(GLint t) {
    switch (t) {
    case GL_RGB8:
        return "GL_RGB8";
    case GL_RGB16F:
        return "GL_RGB16F";
    case GL_RGB32F:
        return "GL_RGB32F";
    case GL_RGBA8:
        return "GL_RGBA8";
    case GL_RGBA16F:
        return "GL_RGBA16F";
    case GL_RGBA32F:
        return "GL_RGBA32F";
    default:
        return "[UNKNOWN GLint]";
        break;
    }
}

std::string to_string(GLenum t)
{
    switch (t)
    {
        case GL_UNSIGNED_INT:
            return "GL_UNSIGNED_INT";
        case GL_FLOAT:
            return "GL_FLOAT";
        case GL_HALF_FLOAT:
            return "GL_HALF_FLOAT";

        case GL_RGB:
            return "GL_RGB";
        case GL_BGR:
            return "GL_BGR";
        case GL_RGBA:
            return "GL_RGBA";
        case GL_BGRA:
            return "GL_BGRA";
        default:
            return "[UNKNOWN GLenum]";
            break;
    }
}


namespace WitOpenEXR
{
    struct Tex {
        GLuint id;
        int width;
        int height;
    };

    struct Pix {
        GLuint id;
        int width;
        int height;
    };

    #if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    inline std::wstring s2ws(const std::string& s)
    {
        int len;
        int slength = (int)s.length() + 1;

        len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
        wchar_t* buf = new wchar_t[len];
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
        std::wstring r(buf);
        delete[] buf;

        return r;
    }
    #endif

    void ProfileSequenceDisplay() {
        ZoneScoped;

        //std::filesystem::path path{ "C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr" };
        //std::filesystem::path path{ "R:/PlasticSky/Render/52_EXAM_v51/52_01_EXAM_v51.0001.exr" };
        //std::filesystem::path path{ "C:/Users/andris/Desktop/52_06_EXAM_v51-raw/52_06_EXAM_v51.0001.exr" };
        std::filesystem::path path{ "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/multipart.0001.exr" };
        auto [pattern, first_frame, last_frame, current_Frame] = scan_for_sequence(path);

        int F = first_frame;
        int mode = 0;
        std::vector<std::string> modes{ "direct upload", "inmutable storage", "pbo" };
        bool swap_textures{ false };

        Imf::setGlobalThreadCount(8);
        std::cout << "global thread count:" << Imf::globalThreadCount() << "\n";

        glazy::init();
        glfwSwapInterval(0);

        static int selected_internal_format_idx{ 1 };
        std::vector<GLint> internalformats{GL_RGB8, GL_RGB16F, GL_RGB32F, GL_RGBA8, GL_RGBA16F, GL_RGBA32F};
        static std::vector<std::string> internalformat_names{"GL_RGB8", "GL_RGB16F", "GL_RGB32F", "GL_RGBA8", "GL_RGBA16F", "GL_RGBA32F"};
        GLint glinternalformat = internalformats[selected_internal_format_idx];

        static int selected_format_idx{ 0 };
        std::vector<GLenum> glformats{ GL_RGB, GL_BGR, GL_RGBA, GL_BGRA};
        std::vector<std::string> formatnames{ "GL_RGB","GL_BGR", "GL_RGBA", "GL_BGRA"};
        GLenum glformat = glformats[selected_format_idx];

        static int selected_type_idx{ 1 };
        std::vector<GLenum> gltypes{ GL_UNSIGNED_INT, GL_HALF_FLOAT, GL_FLOAT };
        std::vector<Imf::PixelType> pixeltypes{ Imf::UINT, Imf::HALF, Imf::FLOAT };
        std::vector<std::string> type_names{ "UINT", "HALF", "FLOAT"};
        GLenum gltype = gltypes[selected_type_idx];
        Imf::PixelType pixeltype = pixeltypes[selected_type_idx];

        // texture buffers
        Tex texIds[2];
        static int texCurrentIndex;
        int texNextIndex;
        for (auto i = 0; i < 2; i++)
        {
            glGenTextures(1, &texIds[i].id);
            glBindTexture(GL_TEXTURE_2D, texIds[i].id);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // pixel buffers
        Pix pbos[2];
        static int pixCurrentIndex{0};
        int pixNextIndex{0};
        for (auto i = 0; i < 2; i++)
        {
            glGenBuffers(1, &pbos[i].id);
        }

        while (glazy::is_running())
        {
            glazy::new_frame();
            // get frame filename
            std::filesystem::path filename;
            {
                ZoneScopedN("get sequence item");
                filename = sequence_item(pattern, F);
                if (!std::filesystem::exists(filename)) {
                    throw "file does not exist";
                }
            }
            
            if (ImGui::Begin("settings"))
            {
                if (ImGui::ListBox("mode", &mode, modes))
                {
                    for (auto i = 0; i < 2; i++)
                    {
                        glDeleteTextures(1, &texIds[i].id);
                        glGenTextures(1, &texIds[i].id);
                        glBindTexture(GL_TEXTURE_2D, texIds[i].id);
                        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }
                }
                ImGui::Checkbox("swap textures", &swap_textures);
                ImGui::Text("sequence: %d / %d-%d", F, first_frame, last_frame);
                ImGui::TextColored(std::filesystem::exists(filename) ? ImColor(1.0f, 1.0f, 1.0f) : ImColor(1.0f, 0.0f, 0.0f), "filename: %s", filename.string().c_str());
                if (ImGui::BeginCombo("internalformat", internalformat_names[selected_internal_format_idx].c_str()))
                {

                    for (auto i = 0; i < internalformat_names.size(); i++) {
                        const bool is_selected = i == selected_internal_format_idx;
                        if (ImGui::Selectable(internalformat_names[i].c_str(), is_selected)) {
                            selected_internal_format_idx = i;
                            glinternalformat = internalformats[selected_internal_format_idx];
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::BeginCombo("data format", formatnames[selected_format_idx].c_str()))
                {

                    for (auto i = 0; i < formatnames.size(); i++) {
                        const bool is_selected = i == selected_format_idx;
                        if (ImGui::Selectable(internalformat_names[i].c_str(), is_selected)) {
                            selected_format_idx = i;
                            glformat = glformats[i];
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::BeginCombo("data type", type_names[selected_type_idx].c_str()))
                {

                    for (auto i = 0; i < type_names.size(); i++) {
                        const bool is_selected = i == selected_type_idx;
                        if (ImGui::Selectable(type_names[i].c_str(), is_selected)) {
                            selected_type_idx = i;
                            gltype = gltypes[i];
                            pixeltype = pixeltypes[i];
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Text("pixeltype: %s", to_string(pixeltypes[selected_type_idx]));
                ImGui::Text("gltype: %s", to_string(gltypes[selected_type_idx]));

                std::cout << "selected_type_idx: " << selected_type_idx << "\n";

                auto ctx = ImGui::GetCurrentContext();
                float* dts = ctx->FramerateSecPerFrame;
                auto offset = ctx->FramerateSecPerFrameIdx;

                float fpss[120];
                for (auto i = 0; i < 120; i++) fpss[i] = 1.0 / dts[i];
                ImGui::PlotHistogram("fps", fpss, 120, offset, "", 0, 120, { 0,70 }, 4);           
            }
            // open file
            Imf::InputFile* file;
            int width=0;
            int height=0;
            char* pixels;
            try{
                #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
                auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
                auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
                file = new Imf::InputFile(*inputStdStream);
                #else
                file = new Imf::InputFile(filename.c_str());
                #endif

                if (ImGui::Begin("settings"))
                {
                    ImGui::TextColored(file->isComplete() ? ImColor(1.f, 0.f, 0.f) : ImColor(1.f,1.f,1.f), "is complete: %s", (file->isComplete() ? "true" : "false"));
                }
                ImGui::End();

                // open header
                auto header = file->header();
                auto dw = file->header().dataWindow();
                width = dw.max.x - dw.min.x + 1;
                height = dw.max.y - dw.min.y + 1;
                int dx = dw.min.x;
                int dy = dw.min.y;

                if (mode == 0)
                {
                    ZoneScopedN("direct upload");
                    {// read pixels
                        ZoneScopedN("read pixels");
                        pixels = (char*)malloc(width * height * 3 * sizeof(float)); //resize
                        //pixels.resizeErase(height, width);
                        Imf::FrameBuffer frameBuffer;

                        std::vector < std::tuple<int, std::string>> channels{ {0, "R"}, {1, "G"}, {2, "B"} };
                        std::vector<unsigned long long> base_sizes(channels.size());
                        for (auto [i, name] : channels) {
                            auto channellist = header.channels();
                            auto channel = channellist[name];
                            base_sizes[i] = pixelTypeSize(channel.type);
                        }

                        auto xstride = sizeof(half) * 3;
                        auto ystride = width * xstride;
                        auto c_offset = 0;
                        for (auto [i, name] : channels)
                        {
                            char* base = pixels - (dy * width + dx) * xstride;
                            auto type_bytes = pixelTypeSize(header.channels()[name].type);
                            std::cout << "!!!!!!!!!! " << to_string(header.channels()[name].type) << " " << type_bytes << "\n";
                            frameBuffer.insert(name,   // name
                                Imf::Slice(Imf::HALF, // type
                                    base + sizeof(half) * i,             // base
                                    xstride, ystride
                                ));
                        }

                        //frameBuffer.insert("A", Imf::Slice(Imf::HALF));

                        // read pixels
                        file->setFrameBuffer(frameBuffer);
                        file->readPixels(dw.min.y, dw.max.y);
                        std::cout << "read pixels: " << "[" << width << "," << height << "]" << " pixeltype: " << to_string(pixeltype) << "\n";
                        //std::cout << "  color: " << pixels[400][400].r << ","<< pixels[400][400].g<<","<< pixels[400][400].b << "\n";
                    }
                    {// Upload to gpu
                        ZoneScopedN("Upload image to GPU mode0");
                        std::cout << "upload to texture: " << texIds[texCurrentIndex].id << "\n";
                        if ( glIsTexture(texIds[texCurrentIndex].id) ) {
                            glDeleteTextures(1,&texIds[texCurrentIndex].id );
                        };
                        glGenTextures(1, &texIds[texCurrentIndex].id);
                        glBindTexture(GL_TEXTURE_2D, texIds[texCurrentIndex].id);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                        
                        
                        //GLenum glformat = GL_RGB;
                        //GLenum gltype = GL_HALF_FLOAT;
                        std::cout << "  glinternalformat: " << to_string(glinternalformat) << "\n"
                            << "  glformat: " << to_string(glformat) << "\n"
                            << "  gltype: " << to_string(gltype) << "\n";
                        
                        glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, width, height, 0, glformat, gltype, pixels);
                        glBindTexture(GL_TEXTURE_2D, 0);

                        //if (swap_textures) {
                        //    texCurrentIndex = (texCurrentIndex + 1) % 2;
                        //    texNextIndex = (texCurrentIndex + 1) % 2;
                        //}
                    }
                    {// Free pixels
                        free(pixels);
                    }
                }
                /*
                else if (mode == 1)
                {
                    ZoneScopedN("inmutable storage");
                    {// read pixels
                        ZoneScopedN("read pixels")
                        pixels.resizeErase(height, width);
                        Imf::FrameBuffer frameBuffer;
                        frameBuffer.insert("R",              // name
                            Imf::Slice(pixeltype,           // type
                                (char*)&pixels[-dy][-dx].r,  // base
                                sizeof(pixels[0][0]) * 1,    // xStride
                                sizeof(pixels[0][0]) * width // yStride
                            ));
                        frameBuffer.insert("G",              // name
                            Imf::Slice(pixeltype,           // type
                                (char*)&pixels[-dy][-dx].g,  // base
                                sizeof(pixels[0][0]) * 1,    // xStride
                                sizeof(pixels[0][0]) * width // yStride
                            ));
                        frameBuffer.insert("B",              // name
                            Imf::Slice(pixeltype,           // type
                                (char*)&pixels[-dy][-dx].b,  // base
                                sizeof(pixels[0][0]) * 1,    // xStride
                                sizeof(pixels[0][0]) * width // yStride
                            ));

                        //frameBuffer.insert("A", Imf::Slice(Imf::HALF));

                        // read pixels
                        file->setFrameBuffer(frameBuffer);
                        file->readPixels(dw.min.y, dw.max.y);
                    }
                    {// Upload to gpu
                        ZoneScopedN("Upload image to GPU mode: ");

                        std::cout << "upload to: " << texIds[texCurrentIndex].id << "\n";
                        glBindTexture(GL_TEXTURE_2D, texIds[texCurrentIndex].id);

                        //GLenum glformat = GL_RGB;
                        //GLenum gltype = GL_HALF_FLOAT;

                        if (texIds[texCurrentIndex].width != width || texIds[texCurrentIndex].height != height)
                        {
                            std::cout << "resize texture storage" << "\n";
                            glBindTexture(GL_TEXTURE_2D, texIds[texCurrentIndex].id);
                            glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
                            texIds[texCurrentIndex].width = width;
                            texIds[texCurrentIndex].height = height;
                        }
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, glformat, gltype, &pixels[0][0].r);
                        glBindTexture(GL_TEXTURE_2D, 0);

                        if (swap_textures) {
                            texCurrentIndex = (texCurrentIndex + 1) % 2;
                            texNextIndex = (texCurrentIndex + 1) % 2;
                        }
                    }
                }
                else if (mode == 2)
                {
                    ZoneScopedN("single pbo");
                    {// read pixels
                        ZoneScopedN("read pixels")
                            pixels.resizeErase(height, width);
                        Imf::FrameBuffer frameBuffer;
                        frameBuffer.insert("R",              // name
                            Imf::Slice(pixeltype,           // type
                                (char*)&pixels[-dy][-dx].r,  // base
                                sizeof(pixels[0][0]) * 1,    // xStride
                                sizeof(pixels[0][0])* width // yStride
                            ));
                        frameBuffer.insert("G",              // name
                            Imf::Slice(pixeltype,           // type
                                (char*)&pixels[-dy][-dx].g,  // base
                                sizeof(pixels[0][0]) * 1,    // xStride
                                sizeof(pixels[0][0])* width // yStride
                            ));
                        frameBuffer.insert("B",              // name
                            Imf::Slice(pixeltype,           // type
                                (char*)&pixels[-dy][-dx].b,  // base
                                sizeof(pixels[0][0]) * 1,    // xStride
                                sizeof(pixels[0][0])* width // yStride
                            ));

                        //frameBuffer.insert("A", Imf::Slice(Imf::HALF));

                        // read pixels
                        file->setFrameBuffer(frameBuffer);
                        file->readPixels(dw.min.y, dw.max.y);
                        //std::cout << "first red: " << pixels[0][0].r << "\n";
                    }
                    {// Upload to gpu
                        ZoneScopedN("Upload image to GPU mode1");

                        std::cout << "uploading to PBO: " << pbos[pixCurrentIndex].id << "\n";
                        //bind current pbo for app->pbo transfer
                        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[pixCurrentIndex].id);
                        glBufferData(GL_PIXEL_UNPACK_BUFFER, width* height * 3, &pixels[0][0].r, GL_STREAM_DRAW);
                        GLfloat* mappedBuffer =(GLfloat*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                        if (mappedBuffer != NULL)
                        {
                            static int color = 0;

                            int* ptr = (int*)mappedBuffer;

                            // copy 4 bytes at once
                            for (int i = 0; i < 50; ++i)
                            {
                                std::cout << i << "\n";
                                for (int j = 0; j < 50; ++j)
                                {
                                   
                                    *ptr = color;
                                    ++ptr;
                                }
                                color += 0.01;
                            }
                            ++color;            // scroll down
                            memcpy(mappedBuffer, &pixels[0][0].r, width* height * 3);
                            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                        }
                        else
                        {
                                std::cout << "null ptr" << "\n";
                        }

                        //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

                        // copy pixels from bpo to texture object
                        std::cout << "upload to texture: " << texIds[texCurrentIndex].id << "\n";
                        glBindTexture(GL_TEXTURE_2D, texIds[texCurrentIndex].id);
                        //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[pixCurrentIndex].id); //bind pbo
                        //GLenum glformat = GL_RGB;
                        //GLenum gltype = GL_HALF_FLOAT;
                        //if (texIds[texCurrentIndex].width != width || texIds[texCurrentIndex].height != height)
                        //{
                        //    std::cout << "resize texture storage" << "\n";
                        //    glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
                        //    texIds[texCurrentIndex].width = width;
                        //    texIds[texCurrentIndex].height = height;
                        //}
                        glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, width, height, 0, glformat, gltype, 0);
                        //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, glformat, gltype, 0);
                        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                        glBindTexture(GL_TEXTURE_2D, 0);

                        if (swap_textures) {
                            texCurrentIndex = (texCurrentIndex + 1) % 2;
                            texNextIndex = (texCurrentIndex + 1) % 2;
                        }
                    }
                    
                }
                */
               
                // Close file
                {
                    ZoneScopedN("delete InputFile");
                    #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
                    delete inputStr;
                    delete inputStdStream;
                    delete file;
                    #else
                    delete file;
                    #endif
                }
            }
            catch (Iex::BaseExc& e)
            {
                std::cerr << e.what() << "\n";
            }

            /// Draw scene
            {
                ZoneScopedN("Draw Scene");
                glClearColor(1, 0, 1, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                imdraw::quad(glazy::checkerboard_tex);
                std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
                imdraw::quad(texIds[texCurrentIndex].id, {-0.9, -0.9}, {0.9, 0.9});
                
            }

            // End frame
            FrameMark;
            F++;
            if (F > last_frame) F = first_frame;
            std::cout << "-----\n";
            glazy::end_frame();

        }
        glazy::destroy();
    }
}
int main()
{
    //WithImageInput::ProfileSequenceDisplay();
    WitOpenEXR::ProfileSequenceDisplay();
}