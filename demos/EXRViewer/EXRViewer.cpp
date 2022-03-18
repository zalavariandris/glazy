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

#include "RenderPlates.h"

const char* PASS_CAMERA_VERTEX_CODE = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;

    out vec3 nearPoint;
    out vec3 farPoint;

    uniform bool use_geometry;

    vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
        mat4 viewInv = inverse(view);
        mat4 projInv = inverse(projection);
        vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
        return unprojectedPoint.xyz / unprojectedPoint.w;
    }
				
    void main()
    {
        nearPoint = UnprojectPoint(aPos.x, aPos.y, 0.0, view, projection).xyz;
        farPoint = UnprojectPoint(aPos.x, aPos.y, 1.0, view, projection).xyz;
        gl_Position = use_geometry ? (projection * view * model * vec4(aPos, 1.0)) : vec4(aPos, 1.0);
    }
)";

const char* PASS_THROUGH_VERTEX_CODE = R"(
                    #version 330 core
                    layout (location = 0) in vec3 aPos;
                    void main()
                    {
                        gl_Position = vec4(aPos, 1.0);
                    }
                    )";

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

    int display_width, display_height;
    GLuint output_tex{ 0 };

private:
    Imf::MultiPartInputFile* first_file = nullptr;
    FileSequence sequence;
    //Imf::Header header;
    void* pixels = NULL;
    std::vector<GLuint> pbos;
    std::vector<std::tuple<int, int, int, int>> pbo_data_sizes;
    bool orphaning{ true };
    

    //int width, height, nchannels;
    //std::string infostring;

    GLenum glinternalformat = GL_RGBA16F;
    GLenum glformat = GL_BGR;
    GLenum gltype = GL_HALF_FLOAT;

    /// Match glformat and swizzle to data
    /// this is a very important step. It depends on the framebuffer channel order.
    /// exr order channel names in aplhebetic order. So by default, ABGR is the read order.
    /// upstream this can be changed, therefore we must handle various channel orders eg.: RGBA, BGRA, and alphebetically sorted eg.: ABGR channel orders.
    static GLenum glformat_from_channels(std::vector<std::string> display_channels, std::array<GLint, 4>& swizzle_mask) {
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

    SequenceRenderer(const FileSequence& seq)
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
        init_pbos(0);
    }
    
    void onGUI()    
    {
        if (ImGui::CollapsingHeader("attributes", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragInt("current frame", &current_frame);
        }

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

    void init_tex() {
        // init texture object
        Imath::Box2i display_window = first_file->header(0).displayWindow();
        display_width = display_window.max.x - display_window.min.x + 1;
        display_height = display_window.max.y - display_window.min.y + 1;
        glPrintErrors();
        glGenTextures(1, &output_tex);
        glBindTexture(GL_TEXTURE_2D, output_tex);
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

        //ImGui::Text("selected part idx: %d", selected_part_idx);
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
        glformat = glformat_from_channels(display_channels, swizzle_mask);

        //ImGui::Text("swizzle mask: %s %s %s %s", to_string(swizzle_mask[0]), to_string(swizzle_mask[1]), to_string(swizzle_mask[2]), to_string(swizzle_mask[3]));
        if (pbos.empty())
        {
            ZoneScopedN("pixels to texture");
            // pixels to texture
            glInvalidateTexImage(output_tex, 1);
            glBindTexture(GL_TEXTURE_2D, output_tex);
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
            glBindTexture(GL_TEXTURE_2D, output_tex);
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
        imdraw::quad(output_tex, {0,0},{ display_width, display_height});
    }

    ~SequenceRenderer() {
        for (auto i = 0; i < pbos.size(); i++) {
            glDeleteBuffers(1, &pbos[i]);
        }
        glDeleteTextures(1, &output_tex);
    }
};

class CorrectionPlate
{
private:
    GLuint mInputtex;
    GLuint fbo;
    GLuint color_attachment;
    int mWidth;
    int mHeight;
    GLuint mProgram;

    int selected_device{ 1 };
    float gain{ 0 };
    float gamma{ 1.0 };
public:
    CorrectionPlate(int width, int height, GLuint inputtex)
    {
        mInputtex = inputtex;
        resize(width, height);
        compile_program();
    }

    void resize(int width, int height)
    {
        ZoneScopedN("update correction fbo");
        if (glIsFramebuffer(fbo))
            glDeleteFramebuffers(1, &fbo);
        if (glIsTexture(color_attachment))
            glDeleteTextures(1, &color_attachment);
        color_attachment = imdraw::make_texture_float(width, height, NULL, GL_RGBA);
        fbo = imdraw::make_fbo(color_attachment);

        mWidth = width;
        mHeight = height;
    }

    void onGui()
    {
        ImGui::BeginGroup(); // display correction
        {
            static const std::vector<std::string> devices{ "linear", "sRGB", "Rec.709" };
            int devices_combo_width = ImGui::CalcComboWidth(devices);
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 6);
            ImGui::SliderFloat(ICON_FA_ADJUST "##gain", &gain, -6.0f, 6.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("gain");
            if (ImGui::IsItemClicked(1)) gain = 0.0;

            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 6);
            ImGui::SliderFloat("##gamma", &gamma, 0, 4);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("gamma");
            if (ImGui::IsItemClicked(1)) gamma = 1.0;


            ImGui::SetNextItemWidth(devices_combo_width);
            ImGui::Combo("##device", &selected_device, "linear\0sRGB\0Rec.709\0");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("device");

        }
        ImGui::EndGroup();
    }

    void set_input_tex(GLuint tex) {
        mInputtex = tex;
    }

    void compile_program()
    {
        const char* display_correction_fragment_code = R"(
            #version 330 core
            out vec4 FragColor;
            uniform mediump sampler2D inputTexture;
            uniform vec2 resolution;

            uniform float gamma_correction;
            uniform float gain_correction;
            uniform int convert_to_device; // 0:linear | 1:sRGB | 2:Rec709

            float sRGB_to_linear(float channel){
                return channel <= 0.04045
                    ? channel / 12.92
                    : pow((channel + 0.055) / 1.055, 2.4);
            }

            vec3 sRGB_to_linear(vec3 color){
                return vec3(
                    sRGB_to_linear(color.r),
                    sRGB_to_linear(color.g),
                    sRGB_to_linear(color.b)
                    );
            }

            float linear_to_sRGB(float channel){
                    return channel <= 0.0031308f
                    ? channel * 12.92
                    : pow(channel, 1.0f/2.4) * 1.055f - 0.055f;
            }

            vec3 linear_to_sRGB(vec3 color){
                return vec3(
                    linear_to_sRGB(color.r),
                    linear_to_sRGB(color.g),
                    linear_to_sRGB(color.b)
                );
            }

            const float REC709_ALPHA = 0.099f;
            float linear_to_rec709(float channel) {
                if(channel <= 0.018f)
                    return 4.5f * channel;
                else
                    return (1.0 + REC709_ALPHA) * pow(channel, 0.45f) - REC709_ALPHA;
            }

            vec3 linear_to_rec709(vec3 rgb) {
                return vec3(
                    linear_to_rec709(rgb.r),
                    linear_to_rec709(rgb.g),
                    linear_to_rec709(rgb.b)
                );
            }


            vec3 reinhart_tonemap(vec3 hdrColor){
                return vec3(1.0) - exp(-hdrColor);
            }

            void main(){
                vec2 uv = gl_FragCoord.xy/resolution;
                vec3 rawColor = texture(inputTexture, uv).rgb;
                
                // apply corrections
                vec3 color = rawColor;

                // apply exposure correction
                color = color * pow(2, gain_correction);

                // exposure tone mapping
                //mapped = reinhart_tonemap(mapped);

                // apply gamma correction
                color = pow(color, vec3(gamma_correction));

                // convert color to device
                if(convert_to_device==0) // linear, no conversion
                {

                }
                else if(convert_to_device==1) // sRGB
                {
                    color = linear_to_sRGB(color);
                }
                if(convert_to_device==2) // Rec.709
                {
                    color = linear_to_rec709(color);
                }

                FragColor = vec4(color, texture(inputTexture, uv).a);
            }
            )";

        ZoneScopedN("recompile display correction shader");
        if (glIsProgram(mProgram)) glDeleteProgram(mProgram);
        mProgram = imdraw::make_program_from_source(PASS_THROUGH_VERTEX_CODE, display_correction_fragment_code);
    }

    void update()
    {
        // update result texture
        BeginRenderToTexture(fbo, 0, 0, mWidth, mHeight);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        imdraw::push_program(mProgram);
        /// Draw quad with fragment shader
        imgeo::quad();
        static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
        static auto vao = imdraw::make_vao(mProgram, { {"aPos", {vbo, 3}} });

        imdraw::set_uniforms(mProgram, {
            {"inputTexture", 0},
            {"resolution", glm::vec2(mWidth, mHeight)},
            {"gain_correction", gain},
            {"gamma_correction", 1.0f / gamma},
            {"convert_to_device", selected_device},
            });
        glBindTexture(GL_TEXTURE_2D, mInputtex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        imdraw::pop_program();
        EndRenderToTexture();
    }

    void draw()
    {
        // draw result texture
        imdraw::quad(color_attachment, { 0,0 }, { mWidth, mHeight });
    }
};


namespace ImGui {
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
            else if (ImGui::IsMouseDragging(0))// && !ImGui::GetIO().KeyMods)
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

    void BeginGLViewer(GLuint* fbo, GLuint *color_attachment, Camera*camera, const ImVec2& size_arg=ImVec2(0,0))
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
        draw_list->AddRect(ImGui::GetWindowPos() + item_pos, ImGui::GetWindowPos() + item_pos + item_size,ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]), ImGui::GetStyle().FrameRounding);

        // Resize texture to viewport size
        int tex_width, tex_height;
        { // get current texture size
            ZoneScopedN("get viewer texture size");
            GLint current_tex;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_tex); // TODO: getting fbo is fairly harmful to performance.
            glBindTexture(GL_TEXTURE_2D, *color_attachment);

            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_height);
            glBindTexture(GL_TEXTURE_2D, current_tex);
        }

        if (tex_width != item_size.x || tex_height != item_size.y)
        { // update fbo size if necessary
            ZoneScopedN("resize viewport fbo");
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
    }

    void EndGLViewer() {
        EndRenderToTexture();
        ImGui::EndGroup();
    }
}

bool run_tests()
{
    return Test::test_layer_class();
}

bool is_playing = false;
int F, first_frame, last_frame;

int selected_viewport_background{ 1 };//0: transparent 1: checkerboard 2:black

FileSequence sequence;
std::unique_ptr<SequenceRenderer> seq_renderer;
std::unique_ptr<LayerManager> layer_manager;
ImGui::GLViewerState viewer_state;

std::unique_ptr<CorrectionPlate> correction_plate;
std::unique_ptr<RenderPlate> polka_plate;
std::unique_ptr<RenderPlate> checker_plate;
std::unique_ptr<RenderPlate> black_plate;

void open(std::filesystem::path filename)
{
    sequence = FileSequence(filename);
    first_frame = sequence.first_frame;
    last_frame = sequence.last_frame;
    F = sequence.first_frame;

    seq_renderer = std::make_unique<SequenceRenderer>(sequence);
    layer_manager = std::make_unique<LayerManager>(sequence.item(sequence.first_frame));
    correction_plate = std::make_unique<CorrectionPlate>(seq_renderer->display_width, seq_renderer->display_height, seq_renderer->output_tex);

    viewer_state.camera.fit(seq_renderer->display_width, seq_renderer->display_height);
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

void update() {
    seq_renderer->update();
    //seq_renderer.draw();
    correction_plate->set_input_tex(seq_renderer->output_tex);
    correction_plate->update();
}

int main(int argc, char* argv[])
{
    for (auto i = 0; i < argc; i++) {
        std::cout << i << ": " << argv[i] << "\n";
    }

    glazy::init();
    glazy::set_vsync(false);
    glfwSetDropCallback(glazy::window, drop_callback);

    if (argc == 2) {
        open(argv[1]);
    }
    else
    {
        open("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");
        //sequence = FileSequence("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");
        //sequence = FileSequence("C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr" );
        //sequence = FileSequence("C:/Users/andris/Desktop/52_EXAM_v51-raw/52_01_EXAM_v51.0001.exr" );
    }
    update();
    polka_plate = std::make_unique<RenderPlate>("./shaders/PASS_THROUGH_CAMERA.vert", "./shaders/polka.frag");
    checker_plate = std::make_unique<RenderPlate>("./shaders/checker.vert", "./shaders/checker.frag");
    black_plate = std::make_unique<RenderPlate>("./shaders/checker.vert", "./shaders/constant.frag");

    while (glazy::is_running())
    {
        glazy::new_frame();
        if (is_playing)
        {
            F++;
            if (F > last_frame) F = first_frame;
            seq_renderer->current_frame = F;

            update();

        }

        if (ImGui::IsKeyPressed('F')) {
            viewer_state.camera.fit(seq_renderer->display_width, seq_renderer->display_height);
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
                    viewer_state.camera.fit(seq_renderer->display_width, seq_renderer->display_height);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (ImGui::Begin("Viewer", (bool*)0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_MenuBar))
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
                            seq_renderer->selected_part_idx = layer_manager->current_part_idx();
                            seq_renderer->selected_channels = layer_manager->current_channel_ids();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("layers");

                ImGui::Separator();

                ImGui::BeginGroup();
                {
                    if (ImGui::Button("fit")) {
                        viewer_state.camera.fit(seq_renderer->display_width, seq_renderer->display_height);
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

                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1, 1));
                        if (ImGui::Selectable(ICON_FA_CHESS_BOARD, selected_viewport_background ==1)) {
                            selected_viewport_background = 1;
                        };

                        if (ImGui::Selectable(" ", selected_viewport_background ==2)) {
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

            ImGui::BeginGLViewer(&viewer_state.viewport_fbo, &viewer_state.viewport_color_attachment, &viewer_state.camera, {-1,-1});
            {
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);
                imdraw::set_projection(viewer_state.camera.getProjection());
                imdraw::set_view(viewer_state.camera.getView());
                glm::mat4 M{ 1 };
                M = glm::scale(M, { seq_renderer->display_width, seq_renderer->display_height, 1 });
                M = glm::translate(M, { 0.5,0.5, 0 });
                //seq_renderer.draw();


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
                //correction_plate->update();
                correction_plate->draw();
            }
            ImGui::EndGLViewer();
        }
        ImGui::End();

        {
            auto viewsize = ImGui::GetMainViewport()->WorkSize;
            ImGui::SetNextWindowSize({ 300, viewsize.y * 2 / 3 });
            ImGui::SetNextWindowPos({ viewsize.x - 320, viewsize.y * 1 / 3 / 2 });
            if (ImGui::Begin("Options", (bool*)0, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav))
            {
                if (ImGui::BeginTabBar("info"))
                {
                    if (ImGui::BeginTabItem("info (current file)"))
                    {
                        auto file = Imf::MultiPartInputFile(sequence.item(seq_renderer->current_frame).string().c_str());
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
                                for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
                                {
                                    //ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    bool is_selected = (seq_renderer->selected_part_idx == p) && std::find(seq_renderer->selected_channels.begin(), seq_renderer->selected_channels.end(), std::string(i.name())) != seq_renderer->selected_channels.end();
                                    if (ImGui::Selectable(i.name(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                                    {
                                        seq_renderer->selected_part_idx = p;
                                        auto channel_name = std::string(i.name());
                                        seq_renderer->selected_channels = { channel_name };
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
                    if (ImGui::BeginTabItem("Settings")) {
                        seq_renderer->onGUI();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
        }

        {
            auto viewsize = ImGui::GetMainViewport()->WorkSize;
            ImGui::SetNextWindowSize({ viewsize.x * 2 / 3,0 });
            ImGui::SetNextWindowPos({ viewsize.x * 1 / 3 / 2, viewsize.y - 80 });
            if (ImGui::Begin("Timeline", (bool*)0, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration))
            {
                if (ImGui::Frameslider("timeslider", &is_playing, &F, first_frame, last_frame)) {
                    seq_renderer->current_frame = F;
                    update();
                }
            }
            ImGui::End();
        }

        glazy::end_frame();
        FrameMark;
    }
    glazy::destroy();
}