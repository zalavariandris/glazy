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


namespace WitOpenEXR
{
    struct RGB {
        half r;
        half g;
        half b;
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
        auto [pattern, first_frame, last_frame, current_Frame] = scan_for_sequence("C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr");
        int F = first_frame;
        int mode = 1;
        std::vector<std::string> modes{ "direct upload", "inmutable storage"};
        bool swap_textures{false};

        Imf::setGlobalThreadCount(4);
        std::cout << "global thread count:" << Imf::globalThreadCount() << "\n";

        glazy::init();
        glfwSwapInterval(0);

        GLuint texA;
        glGenTextures(1, &texA);
        glBindTexture(GL_TEXTURE_2D, texA);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        GLuint texB;
        glGenTextures(1, &texB);
        glBindTexture(GL_TEXTURE_2D, texB);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        int texwidthA, texheightA;
        int texwidthB, texheightB;

        GLuint pboA, pboB;
        glGenBuffers(1, &pboA);
        glGenBuffers(1, &pboB);
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
                if (ImGui::ListBox("mode", &mode, modes)) {
                    
                }
                ImGui::Checkbox("swap textures", &swap_textures);
                ImGui::Text("sequence: %d / %d-%d", F, first_frame, last_frame);
                ImGui::TextColored(std::filesystem::exists(filename) ? ImColor(1.0f, 1.0f, 1.0f) : ImColor(1.0f, 0.0f, 0.0f), "filename: %s", filename.string().c_str());


                float* fpss = ImGui::GetCurrentContext()->FramerateSecPerFrame;

                if (ImPlot::BeginPlot("My Plot")) {
                    ImPlot::PlotBars("My Bar Plot", fpss, 120);
                    //ImPlot::PlotLine("My Line Plot", x_data, y_data, 1000);
                    ImPlot::EndPlot();
                }
                
            }
            // open file
            Imf::InputFile* file;
            int width=0;
            int height=0;
            Imf::Array2D<RGB> pixels;
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
                    ImGui::TextColored(file->isComplete() ? ImColor(1, 0, 0) : ImColor(1,1,1), "%s", (file->isComplete() ? "true" : "false"));
                }
                ImGui::End();

                // open header
                auto dw = file->header().dataWindow();
                width = dw.max.x - dw.min.x + 1;
                height = dw.max.y - dw.min.y + 1;
                int dx = dw.min.x;
                int dy = dw.min.y;

                // read pixels
                {
                    ZoneScopedN("read pixels")

                    // define memory layout
                    pixels.resizeErase(height, width);

                    // define framebuffer
                    Imf::FrameBuffer frameBuffer;
                    frameBuffer.insert("R",              // name
                        Imf::Slice(Imf::HALF,           // type
                            (char*)&pixels[-dy][-dx].r,  // base
                            sizeof(pixels[0][0]) * 1,    // xStride
                            sizeof(pixels[0][0]) * width // yStride
                        ));
                    frameBuffer.insert("G",              // name
                        Imf::Slice(Imf::HALF,           // type
                            (char*)&pixels[-dy][-dx].g,  // base
                            sizeof(pixels[0][0]) * 1,    // xStride
                            sizeof(pixels[0][0]) * width // yStride
                        ));
                    frameBuffer.insert("B",              // name
                        Imf::Slice(Imf::HALF,           // type
                            (char*)&pixels[-dy][-dx].b,  // base
                            sizeof(pixels[0][0]) * 1,    // xStride
                            sizeof(pixels[0][0]) * width // yStride
                        ));

                    //frameBuffer.insert("A", Imf::Slice(Imf::HALF));

                    // read pixels
                    file->setFrameBuffer(frameBuffer);
                    file->readPixels(dw.min.y, dw.max.y);
                    //std::cout << "first red: " << pixels[0][0].r << "\n";
                }

                // Upload to gpu
                if (mode == 0) {
                    ZoneScopedN("Upload image to GPU mode0");
                    glBindTexture(GL_TEXTURE_2D, texA);
                    //GLint glinternalformat = GL_RGB16;
                    //GLenum glformat = GL_RGB;
                    //GLenum gltype = GL_HALF_FLOAT;
                    GLint glinternalformat = GL_RGB16;
                    GLenum glformat = GL_RGB;
                    GLenum gltype = GL_HALF_FLOAT;
                    glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, width, height, 0, glformat, gltype, &pixels[0][0].r);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    if (swap_textures) std::swap(texA, texB);
                }
                else if (mode == 1) {
                    ZoneScopedN("Upload image to GPU mode1");

                    glBindTexture(GL_TEXTURE_2D, texA);

                    GLint glinternalformat = GL_RGB16;
                    GLenum glformat = GL_RGB;
                    GLenum gltype = GL_HALF_FLOAT;

                    if (texwidthA != width || texheightA != height)
                    {
                        std::cout << "resize texture storage" << "\n";
                        glBindTexture(GL_TEXTURE_2D, texA);
                        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
                        texwidthA = width;
                        texheightA = height;
                    }
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, glformat, gltype, &pixels[0][0].r);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    if (swap_textures) {
                        std::swap(texA, texB);
                        std::swap(texwidthA, texwidthB);
                        std::swap(texheightA, texheightB);
                    }
                }
                /*
                else if (mode == 1)
                {
                    ZoneScopedN("Upload image to GPU");
                    glBindTexture(GL_TEXTURE_2D, texA);
                    GLint internalformat = GL_RGB16;
                    GLenum format = GL_RGB;
                    GLenum type = GL_FLOAT;

                    static int texwidth, texheight;
                    if (texwidth != width || texheight != height)
                    {
                        std::cout << "resize texture storage" << "\n";
                        glTexStorage2D(GL_TEXTURE_2D, 0, internalformat, width, height);
                        texwidth = width;
                        texheight = height;
                    }

                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, &pixels[0][0]);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    std::swap(texA, texB);
                }
                else if (mode == 2) 
                {
                    ZoneScopedN("Upload image to GPU");

                    //glBindTexture(GL_TEXTURE_2D, texA);
                    //// resize PBO
                    //static int pbowidth = 0;
                    //static int pboheight = 0;;
                    //if(pbowidth!=width || pboheight!=height) {
                    //    ZoneScopedN("allocate PBO");
                    //    std::cout << "allocate pbos" << "\n";
                    //    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboA);
                    //    glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 3, NULL, GL_STREAM_DRAW);
                    //    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                    //    pbowidth = width;
                    //    pboheight = height;
                    //}

                    //GLint internalformat = GL_RGB16;
                    //GLenum format = GL_RGB;
                    //GLenum type = GL_HALF_FLOAT;

                    //// resize texture
                    //static int texwidth, texheight;
                    //if(texwidth!=width || texheight!=height) {
                    //    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, type, 0);
                    //    texwidth = width;
                    //    texheight = height;
                    //}

                    //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboA);
                    //void* mappedBuffer = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                    //memcpy(mappedBuffer, &pixels[0][0].r, width* height * 3);
                    //glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                

                    //////glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, width * height * 3, &pixels[0][0].r);
                    //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboA);
                    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, NULL);
                    //
                    //glBindTexture(GL_TEXTURE_2D, 0);
                    //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                }
                else if (mode == 3)
                {
                    // see RoundRobin in Asynchronous Buffer Transfers https://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-AsynchronousBufferTransfers.pdf
                    std::swap(pboA, pboB);
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
                imdraw::quad(texA, {-0.9, -0.9}, {0.9, 0.9});
                
            }

            // End frame
            FrameMark;
            F++;
            if (F > last_frame) F = first_frame;
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