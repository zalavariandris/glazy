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
#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfStdIO.h>
#include <OpenEXR/ImfArray.h> //Imf::Array2D
#include <OpenEXR/half.h> // <half> type
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfVersion.h> // get version
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>

#include "helpers.h"

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
    std::string infostring;
    Imf::MultiPartInputFile* firstfile;
    std::unique_ptr<Imf::MultiPartInputFile> current_file;


public:

    SequenceRenderer()
        :width(0), height(0), nchannels(0)
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
        Imf::MultiPartInputFile* file;

        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
        file = new Imf::MultiPartInputFile(*inputStdStream);
        #else
        file = new Imf::MultiPartInputFile(filename.c_str());
        #endif
        firstfile = file;
        // read info to string
        infostring = printInfo(*firstfile);

        /// read header 
        int parts = file->parts();
        bool fileComplete = true;
        for (int i = 0; i < parts && fileComplete; ++i)
            if (!file->partComplete(i)) fileComplete = false;

        header = file->header(0);

        /// alloc 
        Imath::Box2i displayWindow = header.displayWindow();
        width = displayWindow.max.x - displayWindow.min.x + 1;
        height = displayWindow.max.y - displayWindow.min.y + 1;
        nchannels = 4;
        pixels = malloc((size_t)width * height * sizeof(half) * nchannels);
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }

        // init texture object
        init_tex();

        // init pixel buffers
        init_pbos(2);
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
        
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
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

    void read_pixels(int part, std::vector <std::string> channels={"B", "G", "R", "A"}) {
        /// read header
        header = current_file->header(part);

        /// read pixels
        Imath::Box2i dataWindow = header.dataWindow();
        width = dataWindow.max.x - dataWindow.min.x + 1;
        height = dataWindow.max.y - dataWindow.min.y + 1;
        nchannels = 4;
        int dx = dataWindow.min.x;
        int dy = dataWindow.min.y;
  
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        char* buf = (char*)pixels - dx * sizeof(half) * nchannels - dy * (size_t)width * sizeof(half) * nchannels;
        for(auto i=0;i<channels.size();i++){
            auto name = channels[i];
            size_t chanbytes = sizeof(float);
            frameBuffer.insert(name,   // name
                Imf::Slice(Imf::PixelType::HALF, // type
                    buf + i * sizeof(half),           // base
                    sizeof(half) * nchannels,
                    (size_t)width * sizeof(half) * nchannels
                )
            );
            chanoffset += chanbytes;
        }

        // read pixels
        auto part0 = Imf::InputPart(*current_file, part);
        part0.setFrameBuffer(frameBuffer);
        part0.readPixels(dataWindow.min.y, dataWindow.max.y);
    }

    void update(int F, int part=0)
    {
        ZoneScoped;

        auto filename = sequence.item(F);
        if (!std::filesystem::exists(filename)) {
            std::cerr << "file does not exist: " << filename << "\n";
            return;
        }
        
        // open file
        
        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        std::unique_ptr<std::ifstream> inputStr;
        std::unique_ptr<Imf::StdIFStream> inputStdStream;
        #endif
        try 
        {
            #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
            inputStr = std::make_unique<std::ifstream>(s2ws(filename.string()), std::ios_base::binary);
            inputStdStream = std::make_unique<Imf::StdIFStream>(*inputStr, filename.string().c_str());
            current_file = std::make_unique<Imf::MultiPartInputFile>(*inputStdStream);
            #else
            current_file = new Imf::MultiPartInputFile(filename.c_str());
            #endif
        }
        catch (const Iex::InputExc& ex) {
            std::cerr << "file doesn't appear to really be an EXR file" << "\n";
            std::cerr << "  " << ex.what() << "\n";
            return;
        }
       
        read_pixels(part);

        if (pbos.empty())
        {
            // pixels to texture
            glBindTexture(GL_TEXTURE_2D, tex);
            //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_BGR, GL_HALF_FLOAT, (void*)pixels);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_HALF_FLOAT, (void*)pixels);
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
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_HALF_FLOAT, 0/*NULL offset*/);

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

std::vector<std::string> GetExrLayers()
{

}

std::vector<std::string> GetChannelsInLayer()
{

}

namespace Widgets {

    void ChannelSelector(Imf::MultiPartInputFile* file, int& selected_part, std::vector<std::string>& selected_channels)
    {
        std::vector<std::string> part_names;
        auto parts = file->parts();
        for (auto p = 0; p < parts; p++)
        {
            Imf::InputPart in(*file, p);
            part_names.push_back(in.header().hasName() ? in.header().name() : "");
        }

        if (ImGui::BeginCombo("parts", part_names[selected_part].c_str(), ImGuiComboFlags_NoArrowButton))
        {
            for (auto i = 0; i < part_names.size(); i++) {
                const bool is_selected = i == selected_part;
                if (ImGui::Selectable(part_names[i].c_str(), is_selected)) {
                    selected_part = i;
                }

                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("parts");

        // available channels
        std::vector<std::string> available_channels;
        Imf::InputPart in(*file, selected_part);
        auto cl = in.header().channels();
        for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
        {
            available_channels.push_back(i.name());
        }

        if (ImGui::BeginListBox("select channels"))
        {

            for (auto i = 0; i < available_channels.size(); ++i)
            {
                auto name = available_channels[i];
                auto it = std::find(selected_channels.begin(), selected_channels.end(), name);
                const bool is_selected = it != selected_channels.end();
                if (ImGui::Selectable(available_channels[i].c_str(), is_selected))
                {
                    if (is_selected)
                    {
                        selected_channels.erase(it);
                    }
                    else {
                        selected_channels.push_back(available_channels[i]);
                    }
                }
            }
            ImGui::EndListBox();
        }
    }
}

int main()
{
    bool is_playing = false;
    int F, first_frame, last_frame;
    int selected_part{ 0 };
    std::vector<std::string> selected_channels{ "B", "G", "R" };
    Sequence sequence{ "C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr" };
    //Sequence sequence{ "C:/Users/andris/Desktop/52_EXAM_v51-raw/52_01_EXAM_v51.0001.exr" };
    //Sequence sequence{ "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr" };
    first_frame = sequence.first_frame;
    last_frame = sequence.last_frame;
    F = sequence.first_frame;

    glazy::init();

    SequenceRenderer seq_renderer;
    seq_renderer.setup(sequence);

    glazy::set_vsync(false);
    while (glazy::is_running())
    {
        if (is_playing)
        {
            F++;
            if (F > last_frame) F = first_frame;
        }

        glazy::new_frame();

        seq_renderer.update(F, selected_part);

        if (ImGui::Begin("Info"))
        {
            if (ImGui::BeginTabBar("info")) {
                if (ImGui::BeginTabItem("channels"))
                {
                    auto parts = seq_renderer.firstfile->parts();
                    for (auto p = 0; p < parts; p++)
                    {
                        auto in = Imf::InputPart(*seq_renderer.firstfile, p);
                        auto header = in.header();
                        auto cl = header.channels();
                        ImGui::Text("  part %d\n name: %s\n view: %s", p, header.hasName() ? header.name().c_str() : "", header.hasView() ? header.view().c_str() : "");


                        if (ImGui::BeginTable("channels table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingFixedFit))
                        {
                            //ImGui::TableSetupColumn("name");
                            //ImGui::TableSetupColumn("type");
                            //ImGui::TableSetupColumn("sampling");
                            //ImGui::TableSetupColumn("linear");
                            //ImGui::TableHeadersRow();
                            for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("%s", i.name());

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", to_string(i.channel().type).c_str());

                                ImGui::TableNextColumn();
                                ImGui::Text("%d %d", i.channel().xSampling, i.channel().ySampling);

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", i.channel().pLinear ? "true" : "false");
                            }
                            ImGui::EndTable();
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("info"))
                {
                    ImGui::TextWrapped(seq_renderer.infostring.c_str());
                    ImGui::EndTabItem();
                }
            }

        }
        ImGui::End();

        if (ImGui::Begin("Inspector"))
        {
            Widgets::ChannelSelector(seq_renderer.firstfile, selected_part, selected_channels);

            ImGui::Separator();

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
