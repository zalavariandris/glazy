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

// ImGui
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h" // use imgui math operators

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"
#include "glhelpers.h"

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
    bool Frameslider(const char* label, bool* is_playing, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0)
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

    int CalcComboWidth(const std::vector<std::string>& values, ImGuiComboFlags flags = 0)
    {
        int min_width = ImGui::GetTextLineHeight();
        for (const auto& layer : values)
        {
            auto item_width = ImGui::CalcTextSize(layer.c_str()).x;
            if (item_width > min_width) {
                min_width = item_width;
            }
        }
        min_width += ImGui::GetStyle().FramePadding.x * 2;
        if ((flags & ImGuiComboFlags_NoArrowButton) == 0) min_width += ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2;
        return min_width;
    }

    bool CameraControl(Camera* camera, ImVec2 item_size)
    {
        // Control Camera
        bool changed{ false };
        ImGui::InvisibleButton("##camera control", item_size);

        if (ImGui::IsItemActive()) {
            if (ImGui::IsMouseDragging(0) && (ImGui::GetIO().KeyMods == (ImGuiKeyModFlags_Ctrl | ImGuiKeyModFlags_Alt)))
            {
                camera->orbit(-ImGui::GetIO().MouseDelta.x * 0.006, -ImGui::GetIO().MouseDelta.y * 0.006);
                changed = true;
            }
            else if (ImGui::IsMouseDragging(0) || ImGui::IsMouseDragging(2))// && !ImGui::GetIO().KeyMods)
            {
                camera->pan(-ImGui::GetIO().MouseDelta.x / item_size.x, -ImGui::GetIO().MouseDelta.y / item_size.y);
                changed = true;
            }
        }

        if (ImGui::IsItemHovered()) {
            if (ImGui::GetIO().MouseWheel != 0 && !ImGui::GetIO().KeyMods) {
                const auto target_distance = camera->get_target_distance();
                camera->dolly(-ImGui::GetIO().MouseWheel * target_distance * 0.2);
                changed = true;
            }
        }
        return true;
    }

    struct GLViewerState {
        GLuint viewport_fbo;
        GLuint viewport_color_attachment;
        Camera camera{ { 0,0,5000 }, { 0,0,0 }, { 0,1,0 } };
    };

    void BeginGLViewer(GLuint* fbo, GLuint* color_attachment, Camera* camera, const ImVec2& size_arg = ImVec2(0, 0))
    {
        ImGui::BeginGroup();

        // get item rect
        auto item_pos = ImGui::GetCursorPos(); // if child window has no border this is: 0,0
        auto item_size = CalcItemSize(size_arg, 540, 360);

        // add camera control widget
        ImGui::CameraControl(camera, { item_size });

        // add image widget with tecture
        ImGui::SetCursorPos(item_pos);
        ImGui::SetItemAllowOverlap();
        ImGui::Image((ImTextureID)*color_attachment, item_size, ImVec2(0, 1), ImVec2(1, 0));

        // draw border
        auto draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(ImGui::GetWindowPos() + item_pos, ImGui::GetWindowPos() + item_pos + item_size, ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]), ImGui::GetStyle().FrameRounding);

        // Resize texture to viewport size
        int tex_width, tex_height;
        { // get current texture size
            //ZoneScopedN("get viewer texture size");
            GLint current_tex;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_tex); // TODO: getting fbo is fairly harmful to performance.
            glBindTexture(GL_TEXTURE_2D, *color_attachment);

            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_height);
            glBindTexture(GL_TEXTURE_2D, current_tex);
        }

        if (tex_width != item_size.x || tex_height != item_size.y)
        { // update fbo size if necessary
            //ZoneScopedN("resize viewport fbo");
            //std::cout << "update viewport fbo: " << item_size.x << ", " << item_size.y << "\n";
            if (glIsFramebuffer(*fbo))
                glDeleteFramebuffers(1, fbo);
            if (glIsTexture(*color_attachment))
                glDeleteTextures(1, color_attachment);
            *color_attachment = imdraw::make_texture_float(item_size.x, item_size.y, NULL, GL_RGBA);
            *fbo = imdraw::make_fbo(*color_attachment);

            camera->aspect = (float)item_size.x / item_size.y;
        }

        // begin rendering to texture
        BeginRenderToTexture(*fbo, 0, 0, item_size.x, item_size.y);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    }

    void EndGLViewer() {
        EndRenderToTexture();
        ImGui::EndGroup();
    }
}


#include "RenderPlates/RenderPlate.h"
#include "RenderPlates/CorrectionPlate.h"


GLenum glformat_from_channels(std::vector<std::string> channels, std::array<GLint, 4>& swizzle_mask)
{
    /// Match glformat and swizzle to data
    /// this is a very important step. It depends on the framebuffer channel order.
    /// exr order channel names in aplhebetic order. So by default, ABGR is the read order.
    /// upstream this can be changed, therefore we must handle various channel orders eg.: RGBA, BGRA, and alphebetically sorted eg.: ABGR channel orders.

    GLenum glformat;
    if (channels == std::vector<std::string>({ "B", "G", "R", "A" }))
    {
        glformat = GL_BGRA;
        swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    }
    else if (channels == std::vector<std::string>({ "R", "G", "B", "A" }))
    {
        glformat = GL_BGRA;
        swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    }
    else if (channels == std::vector<std::string>({ "A", "B", "G", "R" }))
    {
        glformat = GL_BGRA;
        swizzle_mask = { GL_ALPHA, GL_RED, GL_GREEN, GL_BLUE };
    }
    else if (channels == std::vector<std::string>({ "B", "G", "R" }))
    {
        glformat = GL_BGR;
        swizzle_mask = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
    }
    else if (channels.size() == 4)
    {
        glformat = GL_BGRA;
        swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE , GL_ALPHA };
    }
    else if (channels.size() == 3)
    {
        glformat = GL_RGB;
        swizzle_mask = { GL_RED , GL_GREEN, GL_BLUE, GL_ONE };
    }
    else if (channels.size() == 2) {
        glformat = GL_RG;
        swizzle_mask = { GL_RED, GL_GREEN, GL_ZERO, GL_ONE };
    }
    else if (channels.size() == 1)
    {
        glformat = GL_RED;
        if (channels[0] == "R") swizzle_mask = { GL_RED, GL_ZERO, GL_ZERO, GL_ONE };
        else if (channels[0] == "G") swizzle_mask = { GL_ZERO, GL_RED, GL_ZERO, GL_ONE };
        else if (channels[0] == "B") swizzle_mask = { GL_ZERO, GL_ZERO, GL_RED, GL_ONE };
        else swizzle_mask = { GL_RED, GL_RED, GL_RED, GL_ONE };
    }
    else {
        std::cout << "cannot match selected channels to glformat: ";
        for (auto channel : channels) {
            std::cout << channel << " ";
        }
        std::cout << "\n";
        return -1;
    }
    return glformat;
}


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



bool run_tests()
{
    return Test::test_layer_class();
}


class SequenceReader {
private:
    FileSequence sequence;
    
    // the actial pixels data and display dimensions 
    std::tuple<int, int, int, int> m_bbox; // data window
    int display_x, display_y, display_width, display_height; // display window

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
    int current_frame; // current selected frame
    int selected_part_idx = 0;
    std::vector<std::string> mSelectedChannels{ "A", "B", "G", "R" }; // channels to actually read
    void* pixels;
    SequenceReader(const FileSequence& seq)
    {
        ZoneScoped;

        sequence = seq;
        current_frame = sequence.first_frame;
        Imf::setGlobalThreadCount(24);

        ///
        /// Open file
        /// 
        auto filename = sequence.item(sequence.first_frame);

        auto first_file = std::make_unique<Imf::MultiPartInputFile>(filename.string().c_str());

        // read info to string
        //infostring = get_infostring(*first_file);
        int parts = first_file->parts();
        bool fileComplete = true;
        for (int i = 0; i < parts && fileComplete; ++i)
            if (!first_file->partComplete(i)) fileComplete = false;

        /// read header 
        Imath::Box2i display_window = first_file->header(selected_part_idx).displayWindow();
        display_x = display_window.min.x;
        display_y = display_window.min.y;
        display_width = display_window.max.x - display_window.min.x + 1;
        display_height = display_window.max.y - display_window.min.y + 1;

        init_pixels(display_width, display_height, 4, sizeof(half));
    }

    void init_pixels(int width, int height, int nchannels, size_t typesize)
    {
        pixels = malloc((size_t)width * height * nchannels * sizeof(half));
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }
    }

    void onGUI() {

            int thread_count = Imf::globalThreadCount();
            if (ImGui::InputInt("global thread count", &thread_count)) {
                Imf::setGlobalThreadCount(thread_count);
            }

            ImGui::DragInt("current frame", &current_frame);

    }

    /// display size
    std::tuple<int, int> size()
    {
        return { display_width, display_height };
    }

    std::tuple<int, int, int, int> bbox()
    {
        return m_bbox;
    }

    void set_selected_channels(std::vector<std::string> channels)
    {
        mSelectedChannels = get_display_channels(channels);
    }

    auto selected_channels() {
        return mSelectedChannels;
    }

    void read_to_memory(void* memory)
    {
        ZoneScoped;
        if (mSelectedChannels.empty()) return;

        /// Open Current InputPart
        std::unique_ptr<Imf::MultiPartInputFile> current_file;
        std::unique_ptr<Imf::InputPart> current_inputpart;
        {
            ZoneScopedN("Open Curren InputPart");

            auto filename = sequence.item(current_frame);
            if (!std::filesystem::exists(filename)) {
                std::cerr << "file does not exist: " << filename << "\n";
                return;
            }

            try
            {
                ZoneScopedN("Open Current File");
                current_file = std::make_unique<Imf::MultiPartInputFile>(filename.string().c_str());
            }
            catch (const Iex::InputExc& ex)
            {
                std::cerr << "file doesn't appear to really be an EXR file" << "\n";
                std::cerr << "  " << ex.what() << "\n";
                return;
            }

            {
                ZoneScopedN("seek part");
                current_inputpart = std::make_unique<Imf::InputPart>(*current_file, selected_part_idx);
            }
        }

        /// Update datawindow
        {
            ZoneScopedN("Update datawindow");
            Imath::Box2i dataWindow = current_inputpart->header().dataWindow();
            m_bbox = std::tuple<int, int, int, int>(
                dataWindow.min.x,
                dataWindow.min.y,
                dataWindow.max.x - dataWindow.min.x + 1,
                dataWindow.max.y - dataWindow.min.y + 1);
        }

        // read pixels to pointer
        auto [x, y, w, h] = m_bbox;
        exr_to_memory(*current_inputpart, x, y, w, h, mSelectedChannels, memory);
    }
};

class BBox {
private:
    int x, y, w, h;
public:
    BBox(int x, int y, int width, int height):x(x),y(y), w(width),h(height) {

    }
};

//struct PixelsImage {
//    void* data;
//    std::vector<std::string> channels;
//    std::tuple<int, int, int, int> bbox;
//    Imf::PixelType pixeltype;
//};



//struct TextureImage {
//    GLuint id;
//    std::vector<std::string> channels;
//    std::tuple<int, int, int, int> bbox;
//    Imf::PixelType pixeltype;
//};

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

public:
    PBOImageStream(int width, int height, int channels, int n)
    {
        ZoneScoped;
        pbos.resize(n);

        for (auto i = 0; i < n; i++)
        {
            GLuint pbo;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * channels * sizeof(half), 0, GL_STREAM_DRAW);
            pbos[i].id = pbo;
            pbos[i].bbox = { 0,0, 0,0 };
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        display_index = 0;
        write_index = (display_index + 1) % pbos.size();

        m_width = width;
        m_height = height;
    }

    void onGUI()
    {
        int buffer_size = pbos.size();
        if (ImGui::InputInt("buffer size", &buffer_size)) {
            reformat(m_width, m_height, 4, buffer_size);
        }

        for (auto i=0; i<pbos.size(); i++)
        {
            
            if (write_index == i) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
            }
            if (display_index == i) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
            }
            ImGui::Text("%d", pbos[i].id); ImGui::SameLine();

            if (i == write_index || i == display_index) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::NewLine();
    }

    void reformat(int width, int height, int channels, int n)
    {
        m_width = width;
        m_height = height;
        for (auto i = 0; i < pbos.size(); i++) {
            glDeleteBuffers(1, &pbos[i].id);
        }

        pbos.resize(n);

        for (auto i = 0; i < n; i++)
        {
            GLuint pbo;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * channels * sizeof(half), 0, GL_STREAM_DRAW);
            pbos[i].id = pbo;
            pbos[i].bbox = { 0,0, 0,0 };
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        display_index = 0;
        write_index = (display_index + 1) % pbos.size();
    }

    ~PBOImageStream()
    {
        for (auto i = 0; i < pbos.size(); i++) {
            glDeleteBuffers(1, &pbos[i].id);
        }
    }

    // display PBO bbox
    std::tuple<int,int,int,int> bbox() const
    {
        return pbos.at(display_index).bbox;
    }

    void write(void* pixels, const std::tuple<int, int, int, int>& bbox, const std::vector<std::string>& channels, unsigned long long typesize)
    {
        display_index = (display_index + 1) % pbos.size();
        write_index = (display_index + 1) % pbos.size();

        auto& write_pbo = pbos[write_index];

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, write_pbo.id);
        void* ptr = (void*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        if (ptr)
        {
            auto [x, y, w, h] = bbox;
            memcpy(ptr, pixels, w * h * channels.size() * typesize);
            write_pbo.bbox = bbox;
            write_pbo.channels = channels;
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        else {
            std::cerr << "cannot map PBO" << "\n";
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
    }
};

class PixelsRenderer
{
public:
    GLuint fbo{ 0 };
    GLuint color_attachment{ 0 };
    GLuint mProgram;

    GLuint data_tex;
    int width, height;
    std::tuple<int, int, int, int> m_bbox;


    GLenum glinternalformat = GL_RGBA16F; // select texture internal format

    PixelsRenderer(int width, int height) : width(width), height(height)
    {
        init_tex(width, height);
        init_fbo(width, height);
        init_program();
    }

    void onGUI()
    {
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
                    init_tex(width, height);
                }
            }
            ImGui::EndListBox();
        }
    }

    void init_tex(int width, int height)
    {
        // init texture object
        glPrintErrors();
        glGenTextures(1, &data_tex);
        glBindTexture(GL_TEXTURE_2D, data_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "init tex errors:" << "\n";
        glPrintErrors();
    }

    void init_fbo(int width, int height)
    {
        color_attachment = imdraw::make_texture_float(width, height, 0, glinternalformat, GL_BGRA, GL_HALF_FLOAT);
        fbo = imdraw::make_fbo(color_attachment);
    }

    void init_program() {
        ZoneScoped;
        std::cout << "recompile checker shader" << "\n";

        // reload shader code
        auto vertex_code = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            void main()
            {
                gl_Position = vec4(aPos, 1.0);
            }
        )";

        auto fragment_code = R"(
            #version 330 core
            out vec4 FragColor;
            uniform mediump sampler2D inputTexture;
            uniform vec2 resolution;
            uniform ivec4 bbox;

            void main()
            {
                vec2 uv = (gl_FragCoord.xy)/resolution; // normalize fragcoord
                uv=vec2(uv.x, 1.0-uv.y); // flip y
                uv-=vec2(bbox.x/resolution.x, bbox.y/resolution); // position image
                vec3 color = texture(inputTexture, uv).rgb;
                float alpha = texture(inputTexture, uv).a;
                FragColor = vec4(color, alpha);
            }
        )";

        // link new program
        GLuint program = imdraw::make_program_from_source(
            vertex_code,
            fragment_code
        );

        // swap programs
        if (glIsProgram(mProgram)) glDeleteProgram(mProgram);
        mProgram = program;
    }

    void set_uniforms(std::map<std::string, imdraw::UniformVariant> uniforms) {
        imdraw::set_uniforms(mProgram, uniforms);
    }

    void render_texture_to_fbo()
    {
        ZoneScopedN("datatext to fbo");
        BeginRenderToTexture(fbo, 0, 0, width, height);
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            auto [x, y, w, h] = m_bbox;
            set_uniforms({
                {"inputTexture", 0},
                {"resolution", glm::vec2(width, height)},
                {"bbox", glm::ivec4(x,y,w,h)}
                });

            /// Create geometry
            static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
            static auto vao = imdraw::make_vao(mProgram, { {"aPos", {vbo, 3}} });

            /// Draw quad with fragment shader
            imdraw::push_program(mProgram);
            glBindTexture(GL_TEXTURE_2D, data_tex);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            imdraw::pop_program();
        }
        EndRenderToTexture();
    }

    // from memory
    //static void write_texture(void* pixels, const std::tuple<int,int,int,int>& bbox, const std::vector<std::string>& channels, GLuint tex)
    //{
    //    std::array<GLint, 4> swizzle_mask;
    //    auto glformat = glformat_from_channels(channels, swizzle_mask);
    //    glBindTexture(GL_TEXTURE_2D, tex);
    //    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
    //    auto [x, y, w, h] = bbox;
    //    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, GL_HALF_FLOAT, pixels);
    //    glBindTexture(GL_TEXTURE_2D, 0);
    //}

    // write texture from memory
    void update_from_data(void* pixels, std::tuple<int,int,int,int> bbox, std::vector<std::string> channels, GLenum gltype=GL_HALF_FLOAT)
    {

        
        ZoneScopedN("pixels to texture");
        { // Upload from memory
            std::array<GLint, 4> swizzle_mask;
            auto glformat = glformat_from_channels(channels, swizzle_mask);
            if (glformat == -1) return;

            // update bounding box
            m_bbox = bbox;

            // transfer pixels to texture
            auto [x, y, w, h] = m_bbox; 
            glBindTexture(GL_TEXTURE_2D, data_tex);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, GL_HALF_FLOAT, pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        
        {
            // Draw data_texture to bounding box
            render_texture_to_fbo();
        }
    }

    // write texture from pbo
    void update_from_pbo(GLuint pbo, const std::tuple<int, int, int, int>& bbox, const std::vector<std::string>& channels, GLenum gltype = GL_HALF_FLOAT)
    {
        { // upload from PBO
            std::array<GLint, 4> swizzle_mask;
            auto glformat = glformat_from_channels(channels, swizzle_mask);
            if (glformat == -1) return;

            // update bounding box
            m_bbox = bbox;

            // transfer PBO to texture
            glBindTexture(GL_TEXTURE_2D, data_tex);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            auto [x, y, w, h] = m_bbox;
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, GL_HALF_FLOAT, 0/*NULL offset*/); // orphaning
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        // Draw data_texture to bounding box
        {
            render_texture_to_fbo();
        }
    }
};

bool is_playing = false;
int F, first_frame, last_frame;

int selected_viewport_background{ 1 };//0: transparent 1: checkerboard 2:black

FileSequence sequence;
std::unique_ptr<SequenceReader> reader;
bool use_pbostream{true};
std::unique_ptr<PBOImageStream> pbostream;
std::unique_ptr<LayerManager> layer_manager;
std::unique_ptr<PixelsRenderer> renderer;

ImGui::GLViewerState viewer_state;

std::unique_ptr<CorrectionPlate> correction_plate;
std::unique_ptr<RenderPlate> polka_plate;
std::unique_ptr<RenderPlate> checker_plate;
std::unique_ptr<RenderPlate> black_plate;
bool READ_DIRECTLY_TO_PBO{ true };

bool sidebar_visible{true};

void open(std::filesystem::path filename)
{
    std::cout << "opening: " << filename << "\n";
    if (!std::filesystem::exists(filename)) {
        std::cout << "file does not exist! " << filename << "\n";
    }
    sequence = FileSequence(filename);
    first_frame = sequence.first_frame;
    last_frame = sequence.last_frame;
    F = sequence.first_frame;
    F = sequence.first_frame;

    reader = std::make_unique<SequenceReader>(sequence);
    auto [display_width, display_height] = reader->size();
    pbostream = std::make_unique<PBOImageStream>(display_width, display_height, 4, 3);
    renderer = std::make_unique<PixelsRenderer>(display_width, display_height);
    layer_manager = std::make_unique<LayerManager>(sequence.item(sequence.first_frame));
    correction_plate = std::make_unique<CorrectionPlate>(renderer->width, renderer->height, renderer->color_attachment);

    viewer_state.camera.fit(renderer->width, renderer->height);
}

void drop_callback(GLFWwindow* window, int argc, const char** argv)
{
    for (auto i = 0; i < argc; i++)
    {
        std::cout << "drop file: " << argv[i] << "\n";
    }
    if (argc > 0) {
        open(argv[0]);
    }
}

void update()
{
    auto [display_width, display_height] = reader->size();

    if (use_pbostream)
    {
        if (!READ_DIRECTLY_TO_PBO)
        {
            // read to memory
            static void* pixels = malloc((size_t)display_width * display_height * 4 * sizeof(half));
            if (pixels == NULL) std::cerr << "NULL allocation" << "\n";
            reader->read_to_memory(pixels);

            // memory to pbo
            pbostream->write(pixels, reader->bbox(), reader->selected_channels(), sizeof(half));

            // pbo to texture
            auto& pbo = pbostream->pbos[pbostream->display_index];
            renderer->update_from_pbo(pbo.id, pbo.bbox, pbo.channels, GL_HALF_FLOAT);
        }
        else {
            // read to pbo
            pbostream->display_index = (pbostream->display_index + 1) % pbostream->pbos.size();
            pbostream->write_index = (pbostream->display_index + 1) % pbostream->pbos.size();

            auto& write_pbo = pbostream->pbos[pbostream->write_index];

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, write_pbo.id);
            void* ptr = (void*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr)
            {

                reader->read_to_memory(ptr);

                write_pbo.bbox = reader->bbox();
                write_pbo.channels = reader->selected_channels();
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }
            else {
                std::cerr << "cannot map PBO" << "\n";
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }

            auto& display_pbo = pbostream->pbos[pbostream->display_index];
            renderer->update_from_pbo(display_pbo.id, display_pbo.bbox, display_pbo.channels, GL_HALF_FLOAT);
        }
        
    }
    else
    {
        static void* pixels = malloc((size_t)display_width * display_height * 4 * sizeof(half));
        if (pixels == NULL) std::cerr << "NULL allocation" << "\n";
        reader->read_to_memory(pixels);
        renderer->update_from_data(pixels, reader->bbox(), reader->selected_channels(), GL_HALF_FLOAT);
    }
    correction_plate->set_input_tex(renderer->color_attachment);
    correction_plate->update();
}

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    for (auto i = 0; i < argc; i++) {
        std::cout << i << ": " << argv[i] << "\n";
    }

    // Fix Current Working Directory
    if (argc > 0) {
        try {

            fs::current_path(fs::path(argv[0]).parent_path()); // set working directory to exe
        }
        catch (std::filesystem::filesystem_error const& ex) {
            std::cout
                << "what():  " << ex.what() << '\n'
                << "path1(): " << ex.path1() << '\n'
                << "path2(): " << ex.path2() << '\n'
                << "code().value():    " << ex.code().value() << '\n'
                << "code().message():  " << ex.code().message() << '\n'
                << "code().category(): " << ex.code().category().name() << '\n';
        }
    }
    std::cout << "current working directory: " << fs::current_path() << "\n";
    
    // setup glazy
    glazy::init();
    glazy::set_vsync(false);
    glfwSetDropCallback(glazy::window, drop_callback);

    // Open file
    if (argc == 2)
    {
        // open dropped file
        open(argv[1]);
    }
    else
    {
        // open default sequence
        open("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");
    }

    /// Setup render plates
    polka_plate = std::make_unique<RenderPlate>("PASS_THROUGH_CAMERA.vert", "polka.frag");
    checker_plate = std::make_unique<RenderPlate>("checker.vert", "checker.frag");
    black_plate = std::make_unique<RenderPlate>("checker.vert","constant.frag");

    /// Run main loop
    while (glazy::is_running())
    {
        glazy::new_frame();

        try {

            if (is_playing)
            {
                F++;
                if (F > last_frame) F = first_frame;
                reader->current_frame = F;
            }

            if (ImGui::IsKeyPressed('F')) {
                viewer_state.camera.fit(renderer->width, renderer->height);
            }

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open", "")) {
                        auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg");
                        open(filepath);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View")) {
                    if (ImGui::MenuItem("fit", "f"))
                    {
                        viewer_state.camera.fit(renderer->width, renderer->height);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Windows")) {
                    ImGui::MenuItem("sidebar", "", &sidebar_visible);
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            if (ImGui::Begin("Viewer", (bool*)0, ImGuiWindowFlags_MenuBar))
            {
                if (ImGui::BeginMenuBar())
                {
                    ImGui::SetNextItemWidth(ImGui::CalcComboWidth(layer_manager->names()));
                    if (ImGui::BeginCombo("##layers", layer_manager->current_name().c_str()))\
                    {
                        auto layer_names = layer_manager->names();
                        for (auto i = 0; i < layer_names.size(); i++) {
                            ImGui::PushID(i);
                            bool is_selected = i == layer_manager->current();
                            if (ImGui::Selectable(layer_names.at(i).c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                                layer_manager->set_current(i);
                                reader->selected_part_idx = layer_manager->current_part_idx();
                                reader->set_selected_channels(layer_manager->current_channel_ids());
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("layers");

                    ImGui::Separator();

                    ImGui::BeginGroup();
                    {
                        if (ImGui::Button("fit"))
                        {
                            viewer_state.camera.fit(renderer->width, renderer->height);
                        }
                        if (ImGui::Button("flip y")) {
                            viewer_state.camera = Camera(
                                viewer_state.camera.eye * glm::vec3(1, 1, -1),
                                viewer_state.camera.target * glm::vec3(1, 1, -1),
                                viewer_state.camera.up * glm::vec3(1, -1, 1),
                                viewer_state.camera.ortho,
                                viewer_state.camera.aspect,
                                viewer_state.camera.fovy,
                                viewer_state.camera.tiltshift,
                                viewer_state.camera.near_plane,
                                viewer_state.camera.far_plane
                            );
                        }
                        //ImGui::MenuItem(ICON_FA_CHESS_BOARD, "", &display_checkerboard);
                        ImGui::SetNextItemWidth(ImGui::GetFrameHeightWithSpacing());
                        std::vector<std::string> preview_texts{ ICON_FA_SQUARE_FULL, ICON_FA_CHESS_BOARD ," " };
                        if (selected_viewport_background == 0) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                        }
                        else {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                        }
                        if (ImGui::BeginCombo("##background", preview_texts[selected_viewport_background].c_str(), ImGuiComboFlags_NoArrowButton))
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                            if (ImGui::Selectable(ICON_FA_SQUARE_FULL, selected_viewport_background == 0)) {
                                selected_viewport_background = 0;
                            }
                            ImGui::PopStyleColor();

                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                            if (ImGui::Selectable(ICON_FA_CHESS_BOARD, selected_viewport_background == 1)) {
                                selected_viewport_background = 1;
                            };

                            if (ImGui::Selectable(" ", selected_viewport_background == 2)) {
                                selected_viewport_background = 2;
                            };
                            ImGui::PopStyleColor();
                            ImGui::EndCombo();
                        }
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("background");
                        }
                    }
                    ImGui::EndGroup();

                    ImGui::Separator();
                    correction_plate->onGui();

                    ImGui::EndMenuBar();
                }

                ImGui::BeginGLViewer(&viewer_state.viewport_fbo, &viewer_state.viewport_color_attachment, &viewer_state.camera, { -1,-1 });
                {
                    glClearColor(0, 0, 0, 0);
                    glClear(GL_COLOR_BUFFER_BIT);
                    imdraw::set_projection(viewer_state.camera.getProjection());
                    imdraw::set_view(viewer_state.camera.getView());
                    glm::mat4 M{ 1 };
                    M = glm::scale(M, { renderer->width / 2, renderer->height / 2, 1 });
                    M = glm::translate(M, { 1,1, 0 });

                    if (selected_viewport_background == 1)
                    {
                        checker_plate->set_uniforms({
                            {"projection", viewer_state.camera.getProjection()},
                            {"view", viewer_state.camera.getView()},
                            {"model", M},
                            {"tile_size", 8}
                            });
                        checker_plate->render();
                    }

                    if (selected_viewport_background == 0)
                    {
                        black_plate->set_uniforms({
                            {"projection", viewer_state.camera.getProjection()},
                            {"view", viewer_state.camera.getView()},
                            {"model", M},
                            {"uColor", glm::vec3(0,0,0)}
                            });
                        black_plate->render();
                    }

                    //polka_plate->set_uniforms({
                    //    {"projection", viewer_state.camera.getProjection()},
                    //    {"view", viewer_state.camera.getView()},
                    //    {"model", glm::mat4(1)},
                    //    {"radius", 0.01f}
                    //});
                    //polka_plate->render();
                    correction_plate->render();

                    // display spec
                    
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.5,.5,.5, 0.5));
                        auto width = renderer->width;
                        auto height = renderer->height;
                        
                        auto item_pos = ImGui::GetItemRectMin() - ImGui::GetWindowPos(); // window space
                        auto item_size = ImGui::GetItemRectSize();
                        auto store_cursor = ImGui::GetCursorPos();

                        {
                            
                            glm::vec2 pos = { width, height };

                            glm::vec3 screen_pos = glm::project(glm::vec3(pos, 0.0), viewer_state.camera.getView(), viewer_state.camera.getProjection(), glm::vec4(0, 0, item_size.x, item_size.y));
                            screen_pos.y = item_size.y - screen_pos.y; // invert y

                            ImGui::SetCursorPos({ screen_pos.x + item_pos.x, screen_pos.y + item_pos.y });
                            if (width == 1920 && height == 1080)
                            {
                                ImGui::Text("HD");
                            }
                            else if (width == 3840 && height == 2160)
                            {
                                ImGui::Text("UHD 4K");
                            }
                            else if (width == height)
                            {
                                ImGui::Text("Square %d", width);
                            }
                            else
                            {
                                ImGui::Text("%dx%d", width, height);
                            }
                        }
                        {
                            auto [x, y, w, h] = renderer->m_bbox;
                            glm::vec2 pos = { x, height-y };

                            glm::vec3 screen_pos = glm::project(glm::vec3(pos, 0.0), viewer_state.camera.getView(), viewer_state.camera.getProjection(), glm::vec4(0, 0, item_size.x, item_size.y));
                            screen_pos.y = item_size.y - screen_pos.y; // invert y

                            ImGui::SetCursorPos({ screen_pos.x + item_pos.x, screen_pos.y + item_pos.y });
                            ImGui::Text("%dx%d", x, y);
                        }

                        {
                            auto [x, y, w, h] = renderer->m_bbox;
                            glm::vec2 pos = { x+w, height- (y+h) };
                            
                            glm::vec3 screen_pos = glm::project(glm::vec3(pos, 0.0), viewer_state.camera.getView(), viewer_state.camera.getProjection(), glm::vec4(0, 0, item_size.x, item_size.y));
                            screen_pos.y = item_size.y - screen_pos.y; // invert y

                            ImGui::SetCursorPos({ screen_pos.x + item_pos.x, screen_pos.y + item_pos.y });
                            ImGui::Text("%dx%d", x, y);
                        }
                        ImGui::SetCursorPos(store_cursor); //restore cursor
                        ImGui::PopStyleColor();
                    }                }
                ImGui::EndGLViewer();
            }
            ImGui::End();

            {
                auto viewsize = ImGui::GetMainViewport()->WorkSize;
                ImGui::SetNextWindowSize({ 300, viewsize.y * 2 / 3 });
                ImGui::SetNextWindowPos({ viewsize.x - 320, viewsize.y * 1 / 3 / 2 });

                if (sidebar_visible)
                {
                    if (ImGui::Begin("##Sidebar", &sidebar_visible, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
                    {
                        if (ImGui::BeginTabBar("info"))
                        {
                            if (ImGui::BeginTabItem("info (current file)"))
                            {
                                auto file = Imf::MultiPartInputFile(sequence.item(reader->current_frame).string().c_str());
                                ImGui::TextWrapped(get_infostring(file).c_str());
                                ImGui::EndTabItem();
                            }

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
                                        auto channels = reader->selected_channels();
                                        for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
                                        {
                                            //ImGui::TableNextRow();
                                            ImGui::TableNextColumn();
                                            bool is_selected = (reader->selected_part_idx == p) && std::find(channels.begin(), channels.end(), std::string(i.name())) != channels.end();
                                            if (ImGui::Selectable(i.name(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                                            {
                                                reader->selected_part_idx = p;
                                                auto channel_name = std::string(i.name());
                                                reader->set_selected_channels({ channel_name });
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
                            if (ImGui::BeginTabItem("Settings"))
                            {
                                ImGui::TextWrapped("%s", fs::current_path().string().c_str());

                                if (ImGui::CollapsingHeader("reader", ImGuiTreeNodeFlags_DefaultOpen)) {
                                    reader->onGUI();
                                }
                                if (ImGui::CollapsingHeader("pbostream", ImGuiTreeNodeFlags_DefaultOpen))
                                {
                                    ImGui::Checkbox("use pbostream", &use_pbostream);
                                    
                                    
                                    if (use_pbostream)
                                    {
                                        ImGui::Checkbox("READ DIRECTLY TO PBO", &READ_DIRECTLY_TO_PBO);
                                        pbostream->onGUI();
                                    }
                                }
                                if (ImGui::CollapsingHeader("renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                                    renderer->onGUI();
                                }
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                    }
                    ImGui::End();
                }
            }

            {
                auto viewsize = ImGui::GetMainViewport()->WorkSize;
                ImGui::SetNextWindowSize({ viewsize.x * 2 / 3,0 });
                ImGui::SetNextWindowPos({ viewsize.x * 1 / 3 / 2, viewsize.y - 80 });
                if (ImGui::Begin("Timeline", (bool*)0, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration))
                {
                    if (ImGui::Frameslider("timeslider", &is_playing, &F, first_frame, last_frame)) {
                        reader->current_frame = F;
                    }
                }
                ImGui::End();
            }
            update();
            glazy::end_frame();
            FrameMark;
        }

        catch (const std::exception& ex) {
            std::cout << ex.what() << "\n";
            std::cin.get();
            // ...
        }
        catch (const std::string& ex) {
            std::cout << ex << "\n";
            std::cin.get();
        }
        catch (...) {
            std::cin.get();
        }
    }
    glazy::destroy();
}