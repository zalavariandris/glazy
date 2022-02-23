// EXRViewer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <charconv> // std::to_chars
#include <iostream>
#include "../../tracy/Tracy.hpp"

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"

// OpenEXR
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfStdIO.h>
#include <OpenEXR/ImfArray.h> //Imf::Array2D
#include <OpenEXR/half.h> // <half> type
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfPixelType.h>

class Sequence
{
public:
    Sequence()
    {
        pattern = "";
        first_frame = 0;
        last_frame = 0;
    }

    Sequence(std::filesystem::path filepath) {
        auto [p, b, e, c] = scan_for_sequence(filepath);
        pattern = p;
        first_frame = b;
        last_frame = e;
    }

    std::filesystem::path item(int F)
    {
        if (pattern.empty()) return "";
        return sequence_item(pattern, F);
    }

    std::filesystem::path pattern;
    int first_frame;
    int last_frame;
};

namespace ImGui
{
    bool Frameslider(const char* label, bool *is_playing, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0)
    {
        bool changed = false;
        ImGui::BeginGroup();
        {
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
            if (ImGui::Button(ICON_FA_FAST_BACKWARD "##first frame")) {
                *v = v_min;
                changed = true;
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
            if (ImGui::Button(ICON_FA_STEP_BACKWARD "##step backward")) {
                *v -= 1;
                if (*v < v_min) *v = v_max;
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
            if (ImGui::Button(*is_playing ? ICON_FA_PAUSE : ICON_FA_PLAY "##play"))
            {
                *is_playing = !*is_playing;
                std::cout << "oress play" << *is_playing << "\n";
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
            if (ImGui::Button(ICON_FA_STEP_FORWARD "##step forward")) {
                *v += 1;
                if (*v > v_max) *v = v_min;
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
            if (ImGui::Button(ICON_FA_FAST_FORWARD "##last frame")) {
                *v = v_max;
                changed = true;
            }
        }
        ImGui::EndGroup();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // timeslider
        ImGui::BeginGroup();
        {
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 2);
            ImGui::InputInt("##start frame", &v_min, 0, 0);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::GetTextLineHeight() * 2 - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::SliderInt("##frame", v, v_min, v_max)) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 2);
            ImGui::InputInt("##end frame", &v_max, 0, 0);
            ImGui::EndDisabled();
        }
        ImGui::EndGroup();

        return changed;
    }
    
}


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

class SequenceRenderer
{
public:
    Sequence sequence;
    Imf::Header header;
    void* pixels = NULL;
    std::vector<GLuint> pbos;
    GLuint tex{ 0 };

    int width, height, nchannels;

public:

    SequenceRenderer()
    {

    }

    void onGUI()
    {
        static int npbos = pbos.size();
        if (ImGui::InputInt("pbos", &npbos))
        {
            init_pbos(npbos);
        }
    }

    void setup(Sequence seq)
    {
        ZoneScoped;

        sequence = seq;

        Imf::setGlobalThreadCount(8);

        // open file
        auto filename = sequence.item(sequence.first_frame);
        Imf::InputFile* file;

        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
        file = new Imf::InputFile(*inputStdStream);
        #else
        file = new Imf::InputFile(filename.c_str());
        #endif

        /// read header
        header = file->header();

        /// alloc 
        Imath::Box2i displayWindow = header.displayWindow();
        width = displayWindow.max.x - displayWindow.min.x + 1;
        height = displayWindow.max.y - displayWindow.min.y + 1;
        pixels = malloc((size_t)width * height * sizeof(half) * 3);
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }

        // init texture object
        init_tex();

        // init pixel buffers
        init_pbos(1);
    }

    void init_tex() {
        // init texture object
        glPrintErrors();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        std::cout << width << "x" << height << "\n";
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB16F, width, height);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "setup errors:" << "\n";
        glPrintErrors();
    }

    void init_pbos(int n)
    {
        ZoneScoped;
        TracyMessage(("set pbo count to: "+std::to_string(n)).c_str(), 9);
        if (!pbos.empty())
        {
            for (auto i = 0; i < pbos.size(); i++) {
                glDeleteBuffers(1, &pbos[i]);
            }
            
        }

        pbos.resize(n);
        //glGenBuffers(pbos.size(), pbos.data());
        for (auto i=0; i<n;i++)
        {
            GLuint pbo;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * nchannels * sizeof(half), 0, GL_STREAM_DRAW);
            pbos[i] = pbo;
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void destroy()
    {
        for (auto i = 0; i < pbos.size(); i++) {
            glDeleteBuffers(1, &pbos[i]);
        }
        glDeleteTextures(1, &tex);
    }

    void update(int F)
    {
        ZoneScoped;

        auto filename = sequence.item(F);
        if (!std::filesystem::exists(filename)) {
            std::cerr << "file does not exist: " << filename << "\n";
            return;
        }
        
        // open file
        std::unique_ptr<Imf::InputFile> file;
        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        std::unique_ptr<std::ifstream> inputStr;
        std::unique_ptr<Imf::StdIFStream> inputStdStream;
        #endif
        try 
        {
            #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
            inputStr = std::make_unique<std::ifstream>(s2ws(filename.string()), std::ios_base::binary);
            inputStdStream = std::make_unique<Imf::StdIFStream>(*inputStr, filename.string().c_str());
            file = std::make_unique<Imf::InputFile>(*inputStdStream);
            #else
            file = new Imf::InputFile(filename.c_str());
            #endif
        }
        catch (const Iex::InputExc& ex) {
            std::cerr << "file doesn't appear to really be an EXR file" << "\n";
            std::cerr << "  " << ex.what() << "\n";
            return;
        }
        
        /// read header
        header = file->header();
        
        /// read pixels
        Imath::Box2i dataWindow = header.dataWindow();
        width = dataWindow.max.x - dataWindow.min.x + 1;
        height = dataWindow.max.y - dataWindow.min.y + 1;
        nchannels = 3;
        int dx = dataWindow.min.x;
        int dy = dataWindow.min.y;

        std::vector < std::tuple<int, std::string>> channels{ {0, "B"}, {1, "G"}, {2, "R"} };
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        char* buf = (char*)pixels - dx * sizeof(half) * nchannels - dy * (size_t)width * sizeof(half) * nchannels;
        for (auto [i, name] : channels)
        {
            size_t chanbytes = sizeof(float);
            frameBuffer.insert(name.c_str(),   // name
                Imf::Slice(Imf::PixelType::HALF, // type
                    buf + i * sizeof(half),           // base
                    sizeof(half) * nchannels,
                    (size_t)width * sizeof(half) * nchannels
                )
            );
            chanoffset += chanbytes;
        }

        // read pixels
        file->setFrameBuffer(frameBuffer);
        file->readPixels(dataWindow.min.y, dataWindow.max.y);
        
        // naive PBO
        if (pbos.empty())
        {
            // pixels to texture

            glBindTexture(GL_TEXTURE_2D, tex);
            //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, (void*)pixels);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_HALF_FLOAT, (void*)pixels);
            glBindTexture(GL_TEXTURE_2D, 0);

        }
        else
        {
            static int index{ 0 };
            static int nextIndex;

            index = (index + 1) % pbos.size();
            nextIndex = (index + 1) % pbos.size();

            // pbo to texture
            glBindTexture(GL_TEXTURE_2D, tex);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[index]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_HALF_FLOAT, 0/*NULL offset*/);

            //// pixels to other PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[nextIndex]);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * nchannels * sizeof(half), 0, GL_STREAM_DRAW);
            // map the buffer object into client's memory
            GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr)
            {
                // update data directly on the mapped buffer
                memcpy(ptr, pixels, width * height * nchannels * sizeof(half));
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
            }
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void draw()
    {
        ZoneScoped;
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        //imdraw::quad(glazy::checkerboard_tex);
        //std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
        imdraw::quad(tex, { -0.9, -0.9 }, { 0.9, 0.9 });
    }
};

int main()
{
    bool is_playing = false;
    int F, first_frame, last_frame;
    Sequence sequence{ "C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr" };
    //Sequence sequence{ "C:/Users/andris/Desktop/52_EXAM_v51-raw/52_01_EXAM_v51.0001.exr" };
    first_frame = sequence.first_frame;
    last_frame = sequence.last_frame;
    F = sequence.first_frame;

    glazy::init();

    SequenceRenderer seq_renderer;
    seq_renderer.setup(sequence);

    glazy::set_vsync(false);
    while (glazy::is_running())
    {
        if (is_playing) {
            F++;
            if (F > last_frame) F = first_frame;
        }

        glazy::new_frame();

        seq_renderer.update(F);

        if (ImGui::Begin("Inspector"))
        {
            seq_renderer.onGUI();
        }
        ImGui::End();

        if (ImGui::Begin("Timeline"))
        {
            ImGui::Frameslider("timeslider", &is_playing, &F, first_frame, last_frame);
        }
        ImGui::End();

        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        imdraw::quad(seq_renderer.tex);

        glazy::end_frame();
        FrameMark;
    }
    glazy::destroy();
}

