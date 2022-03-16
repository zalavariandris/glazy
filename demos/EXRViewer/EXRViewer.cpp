// EXRViewer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <vector>
#include <array>
#include <ranges>

#include <format>
#include <iostream>
#include <string>
#include <string_view>

#include <numeric>
#include <charconv> // std::to_chars
#include <iostream>
#include "../../tracy/Tracy.hpp"

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"
#include "glhelpers.h"

#include <imgui_stdlib.h>

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

//
#include "FileSequence.h"
#include "Layer.h"
#include "LayerManager.h"
#include "helpers.h"

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
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() * 2 - ImGui::GetStyle().ItemSpacing.x);
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

template<typename Key, typename Value>
std::vector<Key> extract_keys(std::map<Key, Value> const& input_map) {
    std::vector<Key> retval;
    for (auto const& element : input_map) {
        retval.push_back(element.first);
    }
    return retval;
}

class SequenceRenderer
{
public:
    int current_frame{ 0 };
    Imf::MultiPartInputFile* current_file = nullptr;
    

    int selected_part_idx{ 0 };
    std::vector<std::string> selected_channels{ "A", "B", "G", "R" };
private:
    Imf::MultiPartInputFile* first_file = nullptr;
    FileSequence sequence;
    //Imf::Header header;
    void* pixels = NULL;
    std::vector<GLuint> pbos;
    std::vector<std::tuple<int, int, int, int>> pbo_data_sizes;
    bool orphaning{ true };
    GLuint tex{ 0 };

    int display_width, display_height;

    //int width, height, nchannels;
    //std::string infostring;

    GLenum glinternalformat = GL_RGBA16F;
    GLenum glformat = GL_BGR;
    GLenum gltype = GL_HALF_FLOAT;

    /// Match glformat and swizzle to data
    /// this is a very important step. It depends on the framebuffer channel order.
    /// exr order channel names in aplhebetic order. So by default, ABGR is the read order.
    /// upstream this can be changed, therefore we must handle various channel orders eg.: RGBA, BGRA, and alphebetically sorted eg.: ABGR channel orders.
    static GLenum format_from_channels(std::vector<std::string> display_channels, std::array<GLint, 4>& swizzle_mask) {
        GLenum glformat;
        if (display_channels == std::vector<std::string>({ "B", "G", "R", "A" }))
        {
            glformat = GL_BGRA;
            swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
        }
        else if (display_channels == std::vector<std::string>({ "R", "G", "B", "A" }))
        {
            glformat = GL_BGRA;
            swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
        }
        else if (display_channels == std::vector<std::string>({ "A", "B", "G", "R" }))
        {
            glformat = GL_BGRA;
            swizzle_mask = { GL_ALPHA, GL_RED, GL_GREEN, GL_BLUE };
        }
        else if (display_channels == std::vector<std::string>({ "B", "G", "R" }))
        {
            glformat = GL_BGR;
            swizzle_mask = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
        }
        else if (display_channels.size() == 4)
        {
            glformat = GL_BGRA;
            swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE , GL_ALPHA };
        }
        else if (display_channels.size() == 3)
        {
            glformat = GL_RGB;
            swizzle_mask = { GL_RED , GL_GREEN, GL_BLUE, GL_ONE };
        }
        else if (display_channels.size() == 2) {
            glformat = GL_RG;
            swizzle_mask = { GL_RED, GL_GREEN, GL_ZERO, GL_ONE };
        }
        else if (display_channels.size() == 1)
        {
            glformat = GL_RED;
            if (display_channels[0] == "R") swizzle_mask = { GL_RED, GL_ZERO, GL_ZERO, GL_ONE };
            else if (display_channels[0] == "G") swizzle_mask = { GL_ZERO, GL_RED, GL_ZERO, GL_ONE };
            else if (display_channels[0] == "B") swizzle_mask = { GL_ZERO, GL_ZERO, GL_RED, GL_ONE };
            else swizzle_mask = { GL_RED, GL_RED, GL_RED, GL_ONE };
        }
        else {
            throw "cannot match selected channels to glformat";
        }
        return glformat;
    }

    static int alpha_index(std::vector<std::string> channels) {
        /// find alpha channel index
        int AlphaIndex{ -1 };
        std::string alpha_channel_name{};
        for (auto i = 0; i < channels.size(); i++)
        {
            auto channel_name = channels[i];
            if (ends_with(channel_name, "A") || ends_with(channel_name, "a") || ends_with(channel_name, "alpha")) {
                AlphaIndex = i;
                alpha_channel_name = channel_name;
                break;
            }
        }
        return AlphaIndex;
    }

    static int alpha_channel(const std::vector<std::string>& channels) {
        int AlphaIndex{ -1 };
        std::string alpha_channel_name{};
        for (auto i = 0; i < channels.size(); i++)
        {
            auto channel_name = channels[i];
            if (ends_with(channel_name, "A") || ends_with(channel_name, "a") || ends_with(channel_name, "alpha")) {
                AlphaIndex = i;
                alpha_channel_name = channel_name;
                break;
            }
        }
        return AlphaIndex;
    }

    static std::vector<std::string> get_display_channels(const std::vector<std::string>& channels)
    {
        // Reorder Alpha
        // EXR sort layer names in alphabetically order, therefore the alpha channel comes before RGB
        // if exr contains alpha move this channel to the 4th position or the last position.

        auto AlphaIndex = alpha_channel(channels);

        // move alpha channel to 4th idx
        std::vector<std::string> reordered_channels{ channels };
        if (AlphaIndex >= 0) {
            reordered_channels.erase(reordered_channels.begin() + AlphaIndex);
            reordered_channels.insert(reordered_channels.begin() + std::min((size_t)3, reordered_channels.size()), channels[AlphaIndex]);
        }

        // keep first 4 channels only
        if (reordered_channels.size() > 4) {
            reordered_channels = std::vector<std::string>(reordered_channels.begin(), reordered_channels.begin() + 4);
        }
        return reordered_channels;
    };

public:

    SequenceRenderer()
    {

    }
    
    void onGUI()    
    {
        if (ImGui::CollapsingHeader("data format", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Data format
            ImGui::Text("glformat: %s", to_string(glformat).c_str());
            ImGui::Text("gltype:   %s", to_string(gltype).c_str());
        }

        if (ImGui::CollapsingHeader("OpenGL", ImGuiTreeNodeFlags_DefaultOpen))
        {

            /// PBOS
            static int npbos = pbos.size();
            if (ImGui::InputInt("pbos", &npbos))
            {
                init_pbos(npbos);
            }

            ImGui::Checkbox("orphaning", &orphaning);

            /// Internalformat
            if (ImGui::BeginListBox("texture internal format"))
            {
                std::vector<GLenum> glinternalformats{ GL_RGB16F, GL_RGBA16F, GL_RGB32F, GL_RGBA32F };
                for (auto i = 0; i < glinternalformats.size(); ++i)
                {
                    const bool is_selected = glinternalformat == glinternalformats[i];
                    if (ImGui::Selectable(to_string(glinternalformats[i]).c_str(), is_selected))
                    {
                        glinternalformat = glinternalformats[i];
                        init_tex();
                    }
                }
                ImGui::EndListBox();
            }
        }
    }

    void setup(FileSequence seq)
    {
        ZoneScoped;

        sequence = seq;
        current_frame = sequence.first_frame;
        Imf::setGlobalThreadCount(32);

        ///
        /// Open file
        /// 
        auto filename = sequence.item(sequence.first_frame);

        first_file = new Imf::MultiPartInputFile(filename.string().c_str());

        // read info to string
        //infostring = get_infostring(*first_file);

        /// read header 
        int parts = first_file->parts();
        bool fileComplete = true;
        for (int i = 0; i < parts && fileComplete; ++i)
            if (!first_file->partComplete(i)) fileComplete = false;

        /// alloc 
        Imath::Box2i displayWindow = first_file->header(0).displayWindow();
        auto display_width = displayWindow.max.x - displayWindow.min.x + 1;
        auto display_height = displayWindow.max.y - displayWindow.min.y + 1;
        auto pbo_nchannels = 4;
        pixels = malloc((size_t)display_width * display_height * sizeof(half) * pbo_nchannels);
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
        Imath::Box2i display_window = first_file->header(0).displayWindow();
        display_width = display_window.max.x - display_window.min.x + 1;
        display_height = display_window.max.y - display_window.min.y + 1;
        glPrintErrors();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, display_width, display_height);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "init tex errors:" << "\n";
        glPrintErrors();
    }

    void init_pbos(int n)
    {
        ZoneScoped;

        Imath::Box2i displayWindow = first_file->header(0).displayWindow();
        auto display_width = displayWindow.max.x - displayWindow.min.x + 1;
        auto display_height = displayWindow.max.y - displayWindow.min.y + 1;
        auto max_nchannels = 4;

        TracyMessage(("set pbo count to: "+std::to_string(n)).c_str(), 9);
        if (!pbos.empty())
        {
            for (auto i = 0; i < pbos.size(); i++) {
                glDeleteBuffers(1, &pbos[i]);
            }
            
        }

        pbos.resize(n);
        pbo_data_sizes.resize(n);

        for (auto i=0; i<n;i++)
        {
            GLuint pbo;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, display_width * display_height * max_nchannels * sizeof(half), 0, GL_STREAM_DRAW);
            pbos[i] = pbo;
            pbo_data_sizes[i] = {0,0, 0,0 };
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

    void update()
    {
        ZoneScoped;
        if (selected_channels.empty()) return;
        auto filename = sequence.item(current_frame);
        if (!std::filesystem::exists(filename)) {
            std::cerr << "file does not exist: " << filename << "\n";
            return;
        }

        // open current file

        try
        {
            ZoneScopedN("OpenFile");
            if (current_file != nullptr) delete current_file;
            current_file = new Imf::MultiPartInputFile(filename.string().c_str());

        }
        catch (const Iex::InputExc& ex) {
            std::cerr << "file doesn't appear to really be an EXR file" << "\n";
            std::cerr << "  " << ex.what() << "\n";
            return;
        }

        ImGui::Text("selected part idx: %d", selected_part_idx);
        Imf::InputPart in(*current_file, selected_part_idx);


        /// read pixels to pixels PBO
        Imath::Box2i dataWindow = in.header().dataWindow();
        auto data_width = dataWindow.max.x - dataWindow.min.x + 1;
        auto data_height = dataWindow.max.y - dataWindow.min.y + 1;
        int dx = dataWindow.min.x;
        int dy = dataWindow.min.y;

        auto display_channels = get_display_channels(selected_channels);
        {
            ZoneScopedN("ReadPixels");
            Imf::FrameBuffer frameBuffer;
            //size_t chanoffset = 0;
            unsigned long long xstride = sizeof(half) * display_channels.size();
            char* buf = (char*)pixels - (dx * xstride + dy * data_width * xstride);

            for (auto i = 0; i < display_channels.size(); i++) {
                auto name = display_channels[i];
                //size_t chanbytes = sizeof(half);
                frameBuffer.insert(name,   // name
                    Imf::Slice(Imf::PixelType::HALF, // type
                        buf + i * sizeof(half),           // base
                        xstride,
                        (size_t)data_width * xstride
                    )
                );
                //chanoffset += chanbytes;
            }

            in.setFrameBuffer(frameBuffer);
            in.readPixels(dataWindow.min.y, dataWindow.max.y);
        }

        ///
        /// Transfer pixels to Texture
        /// 

        std::array<GLint, 4> swizzle_mask{ GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
        glformat = format_from_channels(display_channels, swizzle_mask);

        //ImGui::Text("swizzle mask: %s %s %s %s", to_string(swizzle_mask[0]), to_string(swizzle_mask[1]), to_string(swizzle_mask[2]), to_string(swizzle_mask[3]));
        if (pbos.empty())
        {
            ZoneScopedN("pixels to texture");
            // pixels to texture
            glInvalidateTexImage(tex, 1);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, data_width, data_height, glformat, GL_HALF_FLOAT, pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else
        {
            ZoneScopedN("pixels to texture");
            static int index{ 0 };
            static int nextIndex;

            index = (index + 1) % pbos.size();
            nextIndex = (index + 1) % pbos.size();

            //ImGui::Text("using PBOs, update:%d  display: %d", index, nextIndex);

            // pbo to texture
            glBindTexture(GL_TEXTURE_2D, tex);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[index]);
            if(orphaning) glTexSubImage2D(GL_TEXTURE_2D, 0, std::get<0>(pbo_data_sizes[index]), std::get<1>(pbo_data_sizes[index]), std::get<2>(pbo_data_sizes[index]), std::get<3>(pbo_data_sizes[index]), glformat, GL_HALF_FLOAT, 0/*NULL offset*/); // orphaning
            // channel swizzle
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());

            //// pixels to other PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[nextIndex]);
            pbo_data_sizes[nextIndex] = {dx, dy, data_width, data_height };
            glBufferData(GL_PIXEL_UNPACK_BUFFER, data_width * data_height * display_channels.size() * sizeof(half), 0, GL_STREAM_DRAW);
            // map the buffer object into client's memory
            GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr)
            {
                ZoneScopedN("pixels to pbo");
                // update data directly on the mapped buffer
                memcpy(ptr, pixels, data_width * data_height * display_channels.size() * sizeof(half));
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
            }
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void draw()
    {
        if (selected_channels.empty()) return;
        ZoneScoped;
        //glClearColor(0, 0, 0, 0);
        //glClear(GL_COLOR_BUFFER_BIT);
        //imdraw::quad(glazy::checkerboard_tex);
        //std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
        imdraw::quad(tex, {0,0},{ display_width, display_height});
    }

    ~SequenceRenderer() {
        destroy();
    }
};

int main()
{

    //return Test::test_layer_class();

    bool is_playing = false;
    int F, first_frame, last_frame;
    //FileSequence sequence{ "C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr" };
    //FileSequence sequence{ "C:/Users/andris/Desktop/52_EXAM_v51-raw/52_01_EXAM_v51.0001.exr" };
    FileSequence sequence{ "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr" };
    first_frame = sequence.first_frame;
    last_frame = sequence.last_frame;
    F = sequence.first_frame;

    glazy::init();

    SequenceRenderer seq_renderer;
    seq_renderer.setup(sequence);


    LayerManager layer_manager{ sequence.item(sequence.first_frame)};

    glazy::set_vsync(false);
    while (glazy::is_running())
    {
        if (is_playing)
        {
            F++;
            if (F > last_frame) F = first_frame;
            seq_renderer.current_frame = F;
        }

        glazy::new_frame();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("file"))
            {
                if (ImGui::MenuItem("open", "")) {
                    auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg");

                    sequence = FileSequence(filepath);
                    first_frame = sequence.first_frame;
                    last_frame = sequence.last_frame;
                    F = sequence.first_frame;

                    seq_renderer.destroy();
                    seq_renderer.setup(sequence);

                    layer_manager = LayerManager(sequence.item(sequence.first_frame));
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }


        if (ImGui::Begin("Viewer"))
        {
            static GLuint viewport_fbo;
            static GLuint viewport_color_attachment;
            static Camera camera = Camera({ 0,0,5000 }, { 0,0,0 }, { 0,1,0 });
            { /// Viewport
                auto item_size = ImGui::GetContentRegionAvail();
                auto item_pos = ImGui::GetCursorPos(); // if child window has no border this is: 0,0

                // Resize Viewport FBO
                static glm::ivec2 viewport_size;
                if (viewport_size.x != item_size.x || viewport_size.y != item_size.y)
                {
                    ZoneScopedN("update viewport fbo");
                    //std::cout << "update viewport fbo: " << item_size.x << ", " << item_size.y << "\n";
                    viewport_size = { item_size.x, item_size.y };
                    if (glIsFramebuffer(viewport_fbo))
                        glDeleteFramebuffers(1, &viewport_fbo);
                    if (glIsTexture(viewport_color_attachment))
                        glDeleteTextures(1, &viewport_color_attachment);
                    viewport_color_attachment = imdraw::make_texture_float(viewport_size.x, viewport_size.y, NULL, GL_RGBA);
                    viewport_fbo = imdraw::make_fbo(viewport_color_attachment);

                    camera.aspect = (float)viewport_size.x / viewport_size.y;
                }

                // Control Camera
                ImGui::InvisibleButton("camera control", item_size);
                if (ImGui::IsItemActive()) {
                    if (ImGui::IsMouseDragging(0) && (ImGui::GetIO().KeyMods == (ImGuiKeyModFlags_Ctrl | ImGuiKeyModFlags_Alt)))
                    {
                        camera.orbit(-ImGui::GetIO().MouseDelta.x * 0.006, -ImGui::GetIO().MouseDelta.y * 0.006);
                    }
                    else if (ImGui::IsMouseDragging(0))// && !ImGui::GetIO().KeyMods)
                    {
                        camera.pan(-ImGui::GetIO().MouseDelta.x / item_size.x, -ImGui::GetIO().MouseDelta.y / item_size.y);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    if (ImGui::GetIO().MouseWheel != 0 && !ImGui::GetIO().KeyMods) {
                        const auto target_distance = camera.get_target_distance();
                        camera.dolly(-ImGui::GetIO().MouseWheel * target_distance * 0.2);
                    }
                }

                // Display viewport GUI
                ImGui::SetCursorPos(item_pos);
                ImGui::SetItemAllowOverlap();
                ImGui::Image((ImTextureID)viewport_color_attachment, item_size, ImVec2(0, 1), ImVec2(1, 0));

                BeginRenderToTexture(viewport_fbo, 0, 0, item_size.x, item_size.y);
                glClearColor(0, 0, 0,0);
                glClear(GL_COLOR_BUFFER_BIT);
                imdraw::set_projection(camera.getProjection());
                imdraw::set_view(camera.getView());
                glm::mat4 M{ 1 };
                M = glm::scale(M, { 1024, 1024, 1 });
                seq_renderer.draw();
                EndRenderToTexture();
            }

            ImGui::End();
        }

        if (ImGui::Begin("Info"))
        {
            if (ImGui::BeginTabBar("info")) {
                if (ImGui::BeginTabItem("channels"))
                {
                    auto file = Imf::MultiPartInputFile(sequence.item(sequence.first_frame).string().c_str());
                    auto parts = file.parts();
                    for (auto p = 0; p < parts; p++)
                    {
                        ImGui::PushID(p);
                        auto in = Imf::InputPart(file, p);
                        auto header = in.header();
                        auto cl = header.channels();
                        ImGui::LabelText("Part", "%d", p);
                        ImGui::LabelText("Name", "%s", header.hasName() ? header.name().c_str() : "");
                        ImGui::LabelText("View", "%s", header.hasView() ? header.view().c_str() : "");
                        if (ImGui::BeginTable("channels table", 4, ImGuiTableFlags_NoBordersInBody))
                        {
                            ImGui::TableSetupColumn("name");
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("sampling");
                            ImGui::TableSetupColumn("linear");
                            ImGui::TableHeadersRow();
                            for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
                            {
                                //ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                bool is_selected = (seq_renderer.selected_part_idx == p) && std::find(seq_renderer.selected_channels.begin(), seq_renderer.selected_channels.end(), std::string(i.name())) != seq_renderer.selected_channels.end();
                                if (ImGui::Selectable(i.name(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                                {
                                    seq_renderer.selected_part_idx = p;
                                    auto channel_name = std::string(i.name());
                                    seq_renderer.selected_channels = {channel_name};
                                }

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", to_string(i.channel().type).c_str());

                                ImGui::TableNextColumn();
                                ImGui::Text("%d %d", i.channel().xSampling, i.channel().ySampling);

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", i.channel().pLinear ? "true" : "false");
                            }
                            ImGui::EndTable();
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("info (current file)"))
                {
                    ImGui::TextWrapped(get_infostring(*seq_renderer.current_file).c_str());
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

        }
        ImGui::End();

        if (ImGui::Begin("Inspector"))
        {
            if (ImGui::BeginCombo("##layers", layer_manager.current_name().c_str()))\
            {
                auto layer_names = layer_manager.names();
                for (auto i = 0; i < layer_names.size(); i++) {
                    ImGui::PushID(i);
                    bool is_selected = i == layer_manager.current();
                    if (ImGui::Selectable(layer_names.at(i).c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        layer_manager.set_current(i);
                        seq_renderer.selected_part_idx = layer_manager.current_part_idx();
                        seq_renderer.selected_channels = layer_manager.current_channel_ids();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("layers");
        }
        ImGui::End();

        if (ImGui::Begin("Timeline"))
        {
            if (ImGui::Frameslider("timeslider", &is_playing, &F, first_frame, last_frame)) {
                seq_renderer.current_frame = F;
            }
        }
        ImGui::End();

        seq_renderer.update();
        
        seq_renderer.draw();

        glazy::end_frame();
        FrameMark;
    }
    glazy::destroy();
}
