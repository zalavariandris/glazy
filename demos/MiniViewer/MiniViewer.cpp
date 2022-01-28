﻿// MiniViewer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// std libraries
#include <iostream>
#include <set>
#include <cassert>
#include <filesystem>

// std data structures
#include <vector>
#include <string>
#include <unordered_set>

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"

// OpenImageIO
#include "OpenImageIO/imageio.h""
#include "OpenImageIO/imagecache.h"

// utilities
#include "ChannelsTable.h"
#include "Instrumentor.h"
#define PROFILING 1
#if PROFILING
#define PROFILE_SCOPE(name) InstrumentationTimer timer##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__)
#else
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#endif



#include <chrono>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h" // use imgui math operators
#define IMGUI_DEFINE_MATH_OPERATORS

#include <array>

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



class ShaderToy {
private:
    std::filesystem::path fragment_path;
    std::filesystem::file_time_type last_compile_time;
    GLuint m_program;
    glm::ivec2 m_resolution;
    imdraw::UniformVariant uniform;

    std::optional<imgeo::Trimesh> geometry;
    GLuint vbo;
    GLuint ebo;
    GLuint vao;

public:
    ShaderToy(std::filesystem::path fragment_path, glm::ivec2 resolution, std::optional<imgeo::Trimesh> geometry=std::nullopt):
        fragment_path(fragment_path),
        m_program(0),
        m_resolution(resolution),
        geometry(geometry)
    {
        autoreload();
        if (geometry) {
            auto geo = geometry.value();
            vao = imdraw::make_vao(m_program, {
                {"aPos",    {imdraw::make_vbo(geo.positions),      3}},
                {"aUV",     {imdraw::make_vbo(geo.uvs.value()),    2}},
                {"aNormal", {imdraw::make_vbo(geo.normals.value()),3}}
            });
            ebo = imdraw::make_ebo(geo.indices);
        }
        else {
            auto geo = imgeo::quad();
            vao = imdraw::make_vao(m_program, {
                {"aPos",    {imdraw::make_vbo(geo.positions),      3}},
                {"aUV",     {imdraw::make_vbo(geo.uvs.value()),    2}},
                {"aNormal", {imdraw::make_vbo(geo.normals.value()),3}}
             });
            
        }
    }

    // if source files has changed, reload code and recompile shaders
    bool autoreload()
    {
        auto last_write_time = std::filesystem::last_write_time(fragment_path);
        if (last_write_time != last_compile_time) {
            last_compile_time = last_write_time;
            static auto vertex_shader = imdraw::make_shader(GL_VERTEX_SHADER, PASS_CAMERA_VERTEX_CODE);
            if (glIsProgram(m_program)) {
                glDeleteProgram(m_program);
            }
            // read
            auto fragment_code = glazy::read_text(fragment_path.string().c_str());
            // compile
            auto fragment_shader = imdraw::make_shader(GL_FRAGMENT_SHADER, fragment_code.c_str());
            // link
            m_program = imdraw::make_program_from_shaders(vertex_shader, fragment_shader);
            return true;
        }
        return false;
    }

    void draw(std::map<std::string, imdraw::UniformVariant> uniforms)
    {
        imdraw::push_program(m_program);
        /// Draw quad with fragment shader

        imdraw::set_uniforms(m_program, {
            {"projection", uniforms.contains("projection") ? uniforms["projection"] : glm::ortho(-1,1,-1,1)},
            {"view", uniforms.contains("view") ? uniforms["view"] : glm::mat4(1)},
            {"model", uniforms.contains("model") ? uniforms["model"] : glm::mat4(1)},
            {"uResolution", uniforms.contains("resolution") ? uniforms["resolution"] : m_resolution},
            {"use_geometry", geometry.has_value()}
        });
        //glBindTexture(GL_TEXTURE_2D, tex);
        if (geometry.has_value()) {
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glDrawElements(geometry.value().mode, geometry.value().indices.size(), GL_UNSIGNED_INT, NULL);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
        else {
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
        }
        //glBindTexture(GL_TEXTURE_2D, 0);
        imdraw::pop_program();
    }
};



std::vector<GLuint> fbo_stack;
std::vector<std::array<GLint, 4>> viewport_stack;

void BeginRenderToTexture(GLuint fbo, GLint x, GLint y, GLsizei width, GLsizei height)
{
    // push fbo
    GLint current_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo); // TODO: getting fbo is fairly harmful to performance.
    fbo_stack.push_back(current_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // push viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    viewport_stack.push_back({ viewport[0], viewport[1], viewport[2], viewport[3]});
    glViewport(x, y, width, height);
}

void EndRenderToTexture()
{
    ASSERT(("Mismatch Begin/End Rendertargets", !fbo_stack.empty()));

    // restore viewport
    auto last_viewport = viewport_stack.back();
    viewport_stack.pop_back();
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);

    // restore fbo
    GLuint last_fbo = fbo_stack.back();
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
    fbo_stack.pop_back();
}

namespace ImGui {
    struct InputTextCallback_UserData
    {
        std::string* Str;
        ImGuiInputTextCallback  ChainCallback;
        void* ChainCallbackUserData;
    };

    static int InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            // Resize string callback
            // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
            std::string* str = user_data->Str;
            IM_ASSERT(data->Buf == str->c_str());
            str->resize(data->BufTextLen);
            data->Buf = (char*)str->c_str();
        }
        else if (user_data->ChainCallback)
        {
            // Forward to user callback, if any
            data->UserData = user_data->ChainCallbackUserData;
            return user_data->ChainCallback(data);
        }
        return 0;
    }

    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
    }

    bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return InputTextMultiline(label, (char*)str->c_str(), str->capacity() + 1, size, flags, InputTextCallback, &cb_user_data);
    }

    bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return InputTextWithHint(label, hint, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
    }
}


namespace cacheio {
    std::map<std::filesystem::path, std::tuple<std::filesystem::file_time_type, std::string>> texts;
    std::string get_text(const std::filesystem::path& path)
    {
        if (!texts.contains(path))
        {
            auto timestamp = std::filesystem::last_write_time(path);
            auto text = glazy::read_text(path.string().c_str());
            texts[path] = { timestamp, text };
        }
        if (std::get<0>(texts[path]) != std::filesystem::last_write_time(path))
        {
            auto timestamp = std::filesystem::last_write_time(path);
            auto text = glazy::read_text(path.string().c_str());
            texts[path] = { timestamp, text };
        }
        return std::get<1>(texts[path]);
    }
}

GLuint make_texture_from_file(std::filesystem::path filename, std::vector<ChannelKey> channel_keys = {})
{
    PROFILE_FUNCTION();
    /// Validate parameters
    if (channel_keys.empty()) return 0;
    if (!std::filesystem::exists(filename)) return 0;

    /// Read header
    auto image_cache = OIIO::ImageCache::create(true);
    OIIO::ImageSpec spec;
    image_cache->get_imagespec(OIIO::ustring(filename.string()), spec, std::get<0>(channel_keys[0]), 0);
    auto x = spec.x;
    auto y = spec.y;
    int w = spec.width;
    int h = spec.height;
    int chbegin = std::get<1>(channel_keys[0]);
    int chend = std::get<1>(channel_keys[channel_keys.size() - 1]) + 1; // channel range is exclusive [0-3)
    int nchannels = chend - chbegin;

    /// Allocate and read pixels
    float* data = (float*)malloc(w * h * nchannels * sizeof(float));
    image_cache->get_pixels(OIIO::ustring(filename.string()), std::get<0>(channel_keys[0]), 0, x, x + w, y, y + h, 0, 1, chbegin, chend, OIIO::TypeFloat, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, chbegin, chend);

    /// Create texture for ROI
    GLuint tex_roi;
    glGenTextures(1, &tex_roi);
    glBindTexture(GL_TEXTURE_2D, tex_roi);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLint internalformat;
    GLint format;
    if (nchannels == 1) {
        format = GL_RED;
        internalformat = GL_RGBA;
    }
    if (nchannels == 2) {
        format = GL_RG;
        internalformat = GL_RGBA;
    }
    if (nchannels == 3) {
        format = GL_RGB;
        internalformat = GL_RGBA;
    }
    if (nchannels == 4) {
        format = GL_RGBA;
        internalformat = GL_RGBA;
    }
    GLint type = GL_FLOAT;
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, spec.width, spec.height, 0, format, type, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Deallocate data after uploaded to GPU
    free(data);

    /// Render ROI texture to full display window
    GLuint tex_full = imdraw::make_texture_float(spec.full_width, spec.full_height, NULL, GL_RGBA);
    GLuint fbo_full = imdraw::make_fbo(tex_full);
    BeginRenderToTexture(fbo_full, spec.full_x, spec.full_y, spec.full_width, spec.full_height);
    {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        
        imdraw::set_projection(glm::ortho(0.0f, spec.full_width * 1.0f, 0.0f, spec.full_height * 1.0f));
        glm::mat4 M{ 1 };
        imdraw::set_view(glm::mat4(M));

        glm::vec2 min_rect{ spec.x * 1.0f,spec.y * 1.0f };
        glm::vec2 max_rect{ spec.x * 1.0f + spec.width * 1.0f, spec.y * 1.0f + spec.height * 1.0f };
        imdraw::quad(tex_roi, { spec.x * 1.0f,spec.y * 1.0f }, { spec.x * 1.0f + spec.width * 1.0f, spec.y * 1.0f + spec.height * 1.0f });
    }
    EndRenderToTexture();

    // Delete temporary FBO and tetures
    glDeleteFramebuffers(1, &fbo_full);
    glDeleteTextures(1, &tex_roi);
    
    /// Return final texture
    return tex_full;
}

// gui state variables
std::filesystem::path file_pattern{ "" };
int start_frame{ 0 };
int end_frame{ 10 };

int current_frame{ 0 };
bool is_playing{ false };

const double DPI = 300;
Camera camera({ 0,0,5000 }, { 0,0,0 }, { 0,1,0 });

// getters
std::filesystem::path _current_filename; // depends on file_pattern and frame
void update_current_filename() {
    _current_filename = sequence_item(file_pattern, current_frame);
}

ChannelsTable _channels_table; // depends on current filename
void update_channels_table(){
    _channels_table = get_channelstable(_current_filename);
};

std::map<std::string, std::map<std::string, ChannelsTable>> _channels_table_tree;
void update_channels_table_tree() {
    _channels_table_tree.clear();
    for (const auto& [layer, channels] : group_by_layer(_channels_table)) {
        _channels_table_tree[layer] = group_by_view(channels);
    }
};

std::vector<std::string> layers{}; // list of currently available layer names
void update_layers() {
    layers.clear();
    std::unordered_set<std::string> visited;
    for (const auto& [idx, record] : _channels_table) {
        const auto& [layer, view, channel] = record;
        if (!visited.contains(layer)) {
            layers.push_back(layer);
            visited.insert(layer);
        }
    }
};
int current_layer{ 0 }; // currently selected layer index

std::vector<std::string> views{}; // list of currently available view names
void update_views() {
    views.clear();
    for (auto [view_name, df] : _channels_table_tree[layers[current_layer]]) {
        views.push_back(view_name);
    }
};
static int current_view{ 0 }; // currently selected view index

ChannelsTable _current_channels_df;
void update_current_channels_df() {
    _current_channels_df = _channels_table_tree[layers[current_layer]][views[current_view]];
};

std::vector<std::string> channels{}; // list of currently available channel names
void update_channels() {
    channels.clear();
    for (auto [index, record] : _current_channels_df) {
        auto [layer, view, channel_name] = record;
        channels.push_back(channel_name);
    }
};

GLuint tex{ 0 };
void update_texture() {
    if (glIsTexture(tex)) glDeleteTextures(1, &tex);
    tex = make_texture_from_file(_current_filename, get_index_column(_current_channels_df));
};

OIIO::ImageSpec get_current_spec() {
    OIIO::ImageSpec spec;
    auto channel_keys = get_index_column(_current_channels_df);
    if (channel_keys.empty()) return spec;

    auto image_cache = OIIO::ImageCache::create(true);
    auto current_subimage = std::get<0>(channel_keys[0]);
    image_cache->get_imagespec(OIIO::ustring(_current_filename.string()), spec, current_subimage, 0);
    return spec;
}

// actions
void open(std::filesystem::path filepath) {
    if (!filepath.empty()) {
        auto [pattern, start, end] = scan_for_sequence(filepath);
        file_pattern = pattern;
        start_frame = start;
        end_frame = end;

        // keep frame within framerange
        if (current_frame < start_frame) current_frame = start_frame;
        if (current_frame > end_frame) current_frame = end_frame;

        // update filename
        update_current_filename();
        current_layer = 0; // reset to first layer
        current_view = 0; // reset current view
        update_channels_table();
        update_channels_table_tree();
        update_layers();
        update_views();
        update_current_channels_df();
        update_channels();
        update_texture();
    }
};

void fit() {
    // read header
    auto image_cache = OIIO::ImageCache::create(true);
    OIIO::ImageSpec spec;
    auto channel_keys = get_index_column(_current_channels_df);
    image_cache->get_imagespec(OIIO::ustring(_current_filename.string()), spec, std::get<0>(channel_keys[0]), 0);

    camera.eye = { spec.full_width / 2.0 / DPI, spec.full_width / 2.0 / DPI, camera.eye.z };
    camera.target = { spec.full_width / 2.0 / DPI, spec.full_width / 2.0 / DPI, 0.0 };
}

// event handlers
void on_frame_change() {
    update_current_filename();
    update_channels_table();
    update_texture();
};

void on_layer_changed() {
    update_views();
    current_view = 0;
    update_current_channels_df();
    update_channels();
    update_texture();
};

void on_view_change() {
    update_current_channels_df();
    update_channels();
    update_texture();
};

void ShowChannelsTable()
{
    if (ImGui::BeginTabBar("image header"))
    {
        
        if (ImGui::BeginTabItem("all channels"))
        {
            if (ImGui::BeginTable("channels table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("index");
                ImGui::TableSetupColumn("layer");
                ImGui::TableSetupColumn("view");
                ImGui::TableSetupColumn("channel");
                ImGui::TableHeadersRow();
                for (const auto& [subimage_chan, layer_view_channel] : _channels_table) {
                    auto [subimage, chan] = subimage_chan;
                    auto [layer, view, channel] = layer_view_channel;

                    //ImGui::TableNextRow();
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%d/%d", subimage, chan);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", layer.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", view.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text(channel.c_str());
                    //ImGui::Text("%s", channel.c_str());
                }
                ImGui::TableNextRow();
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("layers"))
        {
            ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
            for (auto [layer, view_groups] : _channels_table_tree) {
                //ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                
                if (ImGui::TreeNodeEx(layer.c_str(), node_flags))
                {
                    for (auto [view, df] : view_groups) {
                        //ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                        
                        if (ImGui::TreeNodeEx(view.c_str(), node_flags))
                        {
                            for (auto [index, record] : df){
                                auto [layer, view, channel] = record;
                                if (channel == "R" || channel == "x") ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(240, 0, 0));
                                else if (channel == "G" || channel == "y") ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 240, 0));
                                else if (channel == "B" || channel == "z") ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(60, 60, 255));
                                else if (channel == "A") ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(240, 240,240));
                                else ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(180, 180, 180));
                                ImGui::Text("%s", channel.c_str());
                                ImGui::PopStyleColor();
                                ImGui::SameLine();
                            }ImGui::NewLine();

                            ImGui::TreePop();
                        }
                        
                    }
                    ImGui::TreePop();
                }
                

            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void ShowImageInfo() {
    if (ImGui::CollapsingHeader("sequence", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("folder: %s", file_pattern.parent_path().string().c_str());
        ImGui::Text("filename: %s", file_pattern.filename().string().c_str());
        ImGui::Text("framerange: %d-%d", start_frame, end_frame);
        ImGui::Text("(%s)", _current_filename.filename().string().c_str());
    }

    if (ImGui::CollapsingHeader("overall spec", ImGuiTreeNodeFlags_DefaultOpen))
    {
        /*
        auto in = OIIO::ImageInput::open(_current_filename.string());
        bool exhausted{ false };
        while (!exhausted)
        {
        }
        while (in->seek_subimage(0, nmipmaps))
        {
            ++nmipmaps;
        }
        const OIIO::ImageSpec& spec = in->spec(subimage, mipmap);
        */
        // read header
        auto image_cache = OIIO::ImageCache::create(true);
        OIIO::ImageSpec spec;
        image_cache->get_imagespec(OIIO::ustring(_current_filename.string()), spec, 0, 0);
        std::string info = spec.serialize(OIIO::ImageSpec::SerialText, OIIO::ImageSpec::SerialDetailedHuman);
        ImGui::Text("%s", info.c_str());
    }
}

void ShowTimeline() {
    // Timeslider
    ImGui::BeginGroup();
    {
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_FAST_BACKWARD "##first frame")) {
            current_frame = start_frame;
            on_frame_change();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_STEP_BACKWARD "##step backward")) {
            current_frame -= 1;
            if (current_frame < start_frame) current_frame = end_frame;
            on_frame_change();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(is_playing ? ICON_FA_PAUSE : ICON_FA_PLAY "##play")) {
            is_playing = !is_playing;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_STEP_FORWARD "##step forward")) {
            current_frame += 1;
            if (current_frame > end_frame) current_frame = start_frame;
            on_frame_change();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_FAST_FORWARD "##last frame")) {
            current_frame = end_frame;
            on_frame_change();
        }
    }
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    {
        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 2);
        ImGui::InputInt("##start frame", &start_frame, 0, 0);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::GetTextLineHeight() * 2 - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SliderInt("##frame", &current_frame, start_frame, end_frame)) {
            on_frame_change();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 2);
        ImGui::InputInt("##end frame", &end_frame, 0, 0);
        ImGui::EndDisabled();
    }
    ImGui::EndGroup();
}

struct RenderTarget {
    GLuint fbo=0;
    GLuint color_attachment=0;
    int width=0;
    int height=0;
};

RenderTarget make_rendertarget(int width, int height) {
    RenderTarget rt;
    rt.color_attachment = imdraw::make_texture_float(width, height, NULL, GL_RGBA);
    rt.fbo = imdraw::make_fbo(rt.color_attachment);
    rt.width = width;
    rt.height = height;
    return rt;
}

void delete_rendertarget(const RenderTarget & rt) {
    glDeleteTextures(1, &rt.color_attachment);
    glDeleteFramebuffers(1, &rt.fbo);
}

void ShowMiniViewer(bool *p_open) {
    static float gain = 1.0;
    static float gamma = 1.0;
    static bool display_checkerboard=true;

    if (ImGui::Begin("Viewer", p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar)) {

        if (ImGui::BeginMenuBar())
        {
            // Toolbar
            ImGui::BeginGroup(); // Channel secetion group
            {
                int combo_width = ImGui::GetTextLineHeight()*2;
                for (const auto& layer : layers) {
                    auto w = ImGui::CalcTextSize(layer.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                    if (w > combo_width) {
                        combo_width = w;
                    }
                }
                ImGui::SetNextItemWidth(combo_width+ ImGui::GetTextLineHeight());
                if (ImGui::BeginCombo("##layers", layers.size()>0 ? layers[current_layer].c_str() : "-", ImGuiComboFlags_NoArrowButton)) {
                    for (auto i = 0; i < layers.size(); i++) {
                        const bool is_selected = layers[i] == layers[current_layer];
                        if (ImGui::Selectable(layers[i].c_str(), is_selected)) {
                            current_layer = i;
                            on_layer_changed();
                        }

                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                //ImGui::SameLine();
                combo_width = ImGui::GetTextLineHeight() * 2;
                for (const auto& name : views) {
                    auto w = ImGui::CalcTextSize(name.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                    if (w > combo_width) {
                        combo_width = w;
                    }
                }
                ImGui::SetNextItemWidth(combo_width + ImGui::GetTextLineHeight());
                if (views.size() == 1)
                {
                    if (!views[current_view].empty())
                    {
                        ImGui::Text(views[current_view].c_str());
                    }
                    else {
                    }
                }
                else if (views.size() > 1)
                {
                    if (ImGui::Combo("##views", &current_view, views)) {
                        on_view_change();
                    }
                }

                //ImGui::SameLine();

                for (auto chan : channels) {
                    ImGui::MenuItem(chan.c_str());
                };
            }
            ImGui::EndGroup(); // toolbar end
            
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::BeginGroup(); // display correction
            {
                ImGui::Text(ICON_FA_ADJUST);
                ImGui::SetNextItemWidth(120);
                ImGui::SliderFloat("##gain", &gain, -6.0f, 6.0f);
                if (ImGui::IsItemClicked(1)) gain = 0.0;

                ImGui::Text("Gamma");
                ImGui::SetNextItemWidth(120);
                ImGui::SliderFloat("γ##gamma", &gamma, 0, 4);
                if (ImGui::IsItemClicked(1)) gamma = 1.0;
            }
            ImGui::EndGroup();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::MenuItem(ICON_FA_CHESS_BOARD, "", &display_checkerboard);
        ImGui::EndMenuBar();
        }

        // Viewport
        {
            static RenderTarget viewport_rt;
            //static bool flip_y{ false };
            //if (ImGui::Checkbox("flip", &flip_y)) {
            //    camera = Camera({ 0,0,flip_y ? -500 : 500 }, { 0,0,0 }, { 0,flip_y ? -1 : 1,0 }, false, camera.aspect, camera.fov);
            //}

            auto item_pos = ImGui::GetCursorPos();
            auto item_size = ImGui::GetContentRegionAvail();

            // Resize Viewport FBO
            static ImVec2 display_size;
            if (display_size.x != item_size.x || display_size.y != item_size.y)
            {
                std::cout << "update fbo: " << item_size.x << ", " << item_size.y << "\n";
                display_size = item_size;
                delete_rendertarget(viewport_rt);
                viewport_rt = make_rendertarget(item_size.x, item_size.y);
                camera.aspect = item_size.x / item_size.y;
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
            ImGui::Image((ImTextureID)viewport_rt.color_attachment, item_size, ImVec2(0, 1), ImVec2(1, 0));

            OIIO::ImageSpec spec = get_current_spec();
            //if (spec.full_width > 0)

            //{
            //    glm::vec2 pos = glm::project(glm::vec3(spec.full_width, spec.full_height, 0), camera.getView(), camera.getProjection(), glm::vec4(0, 0, item_size.x, item_size.y));
            //    //pos *= glm::vec2(item_size.x, item_size.y);
            //    ImGui::SetCursorPos(ImVec2(pos.x + item_pos.x, item_size.y - pos.y + item_pos.y));
            //    ImGui::Text("%dx%d", spec.full_width, spec.full_height);
            //}

            ImGui::SetCursorPos(item_pos);



            // Make Image Correction texture
            static RenderTarget correction_rt;
            if (correction_rt.width != spec.full_width || correction_rt.height != spec.full_height) {
                std::cout << "create corection fbo" << std::endl;
                correction_rt = make_rendertarget(spec.full_width, spec.full_height);
            }

            const char * PASS_THROUGH_VERTEX_CODE = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            void main()
            {
                gl_Position = vec4(aPos, 1.0);
            }
            )";

            static GLuint correction_program;
            if (glazy::is_file_modified("display_correction.frag")) {
                std::string correction_fragment_code = glazy::read_text("display_correction.frag");
                correction_program = imdraw::make_program_from_source(PASS_THROUGH_VERTEX_CODE, correction_fragment_code.c_str());
            }

            // render to correction fbo
            BeginRenderToTexture(correction_rt.fbo, 0, 0, spec.full_width, spec.full_height);
            {
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);

                imdraw::push_program(correction_program);
                /// Draw quad with fragment shader
                imgeo::quad();
                static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
                static auto vao = imdraw::make_vao(correction_program, { {"aPos", {vbo, 3}} });

                imdraw::set_uniforms(correction_program, {
                    {"inputTexture", 0},
                    {"resolution", glm::vec2(spec.full_width, spec.full_height)},
                    {"gamma_correction", gamma},
                    {"gain_correction", gain}
                });
                glBindTexture(GL_TEXTURE_2D, tex);
                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
                imdraw::pop_program();
            }
            EndRenderToTexture();

            // Render to Viewport FBO
            BeginRenderToTexture(viewport_rt.fbo, 0, 0, item_size.x, item_size.y);
            {
                glClearColor(0.0, 0.0, 0.0, 0.0);
                glClear(GL_COLOR_BUFFER_BIT);
                imdraw::set_projection(camera.getProjection());
                imdraw::set_view(camera.getView());

                /// Draw scene
                // draw background
                {


                    static std::filesystem::path fragment_path{ "./polka.frag" };
                    static std::string fragment_code;
                    static GLuint polka_program;
                    if (glazy::is_file_modified(fragment_path)) {
                        // reload shader
                        fragment_code = glazy::read_text(fragment_path.string().c_str());
                        if (glIsProgram(polka_program)) glDeleteProgram(polka_program); // recompile shader
                        polka_program = imdraw::make_program_from_source(
                            PASS_CAMERA_VERTEX_CODE,
                            fragment_code.c_str()
                        );
                    }

                    /// Draw quad with fragment shader
                    static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
                    static auto vao = imdraw::make_vao(polka_program, { {"aPos", {vbo, 3}} });

                    imdraw::push_program(polka_program);
                    imdraw::set_uniforms(polka_program, {
                        {"projection", camera.getProjection()},
                        {"view", camera.getView()},
                        {"model", glm::mat4(1)},
                        {"uResolution", glm::vec2(item_size.x, item_size.y)},
                        {"radius", 0.01f}
                        });

                    glBindVertexArray(vao);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                    glBindVertexArray(0);
                    imdraw::pop_program();
                }
                
                // draw image
                if (!_current_channels_df.empty()) {

                    // read header
                    auto image_cache = OIIO::ImageCache::create(true);
                    OIIO::ImageSpec spec;
                    auto channel_keys = get_index_column(_current_channels_df);
                    int SUBIMAGE = std::get<0>(channel_keys[0]); //todo: ALL SUBIMAGE MUST MATCH
                    image_cache->get_imagespec(OIIO::ustring(_current_filename.string()), spec, SUBIMAGE, 0);
                    int chbegin = std::get<1>(channel_keys[0]);
                    int chend = std::get<1>(channel_keys[channel_keys.size() - 1]) + 1; // channel range is exclusive [0-3)
                    int nchannels = chend - chbegin;
                    glm::vec2 min_rect{ spec.full_x, spec.full_y };
                    glm::vec2 max_rect{ spec.full_x + spec.full_width, spec.full_y + spec.full_height };



                    // draw image background
                    if (display_checkerboard)
                    { // draw checkerboard background
                        glDisable(GL_BLEND);
                        static auto toy = ShaderToy("./try_shadertoy.frag", { 512,512 }, imgeo::rect(min_rect, max_rect));
                        toy.autoreload();
                        toy.draw({
                            { "projection", camera.getProjection() },
                            { "view", camera.getView() },
                            { "model", glm::scale(glm::mat4(1), glm::vec3(1))}
                            });
                        glEnable(GL_BLEND);
                    }
                    else
                    { // draw black background
                        imdraw::rect(
                            { spec.full_x,spec.full_y }, // min rect
                            { spec.full_x + spec.full_width, spec.full_y + spec.full_height }, //max rect
                            { //Material
                                .color = glm::vec3(0,0,0)
                            }
                        );
                    }

                    // draw textured quad
                    imdraw::quad(correction_rt.color_attachment,
                        min_rect,
                        max_rect
                    );

                    /// Draw boundaries
                    // full image boundaries
                    imdraw::rect(
                        min_rect,
                        max_rect,
                        { .mode = imdraw::LINE, .color=glm::vec3(0.5), .opacity=0.3}
                    );

                    // data window boundaries
                    imdraw::rect(
                        { spec.x,spec.y }, 
                        { spec.x + spec.width, spec.y + spec.height },
                        { .mode = imdraw::LINE, .color=glm::vec3(0.5), .opacity=0.3}
                    );
                }

                //imdraw::grid();
                imdraw::axis(100);

            }
            EndRenderToTexture();
        }
        
    }
    ImGui::End(); // End Viewer window
}

int main()
{
    Instrumentor::Get().BeginSession("Profile");
    static bool image_viewer_visible{ true };
    static bool image_info_visible{ true };
    static bool channels_table_visible{ true };
    static bool timeline_visible{ true };
    glazy::init();
    while (glazy::is_running())
    {
        PROFILE_SCOPE("frame");
        // control playback
        if (is_playing) {
            current_frame++;
            if (current_frame > end_frame) {
                current_frame = start_frame;
            }
            on_frame_change();
        }

        // GUI
        glazy::new_frame();
        
        // Main menubar
        
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open")) {
                    auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg");
                    open(filepath);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if(ImGui::MenuItem("Fit", "f")) {
                    fit();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::MenuItem("image viewer", "", &image_viewer_visible);
                ImGui::MenuItem("image info", "", &image_info_visible);
                ImGui::MenuItem("image channels table", "", &channels_table_visible);
                ImGui::EndMenu();
            }
            ImGui::Spacing();
            ImGui::Text("%s", file_pattern.string().c_str());
            
            ImGui::EndMainMenuBar();
        }
        
        // Image Viewer
        if (image_viewer_visible) {
            PROFILE_SCOPE("image viewer");
            ShowMiniViewer(&image_viewer_visible);
        }
        
        // ChannelsTable

        if (channels_table_visible) {
            PROFILE_SCOPE("channels table");
            if (ImGui::Begin("ChannelsTtable", &channels_table_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
                ShowChannelsTable();
            }
            ImGui::End();
        }
        
        // Image info
        ImGui::SetNextWindowSizeConstraints(ImVec2(100, -1), ImVec2(200, -1));          // Width 400-500
        if (image_info_visible) {
            if (ImGui::Begin("Info", &image_info_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
                ShowImageInfo();
            }
            ImGui::End();
        }

        // Timeliune
        if (timeline_visible){
            if (ImGui::Begin("timeline")) {
                ShowTimeline();
            }
            ImGui::End();
        }
        
        glazy::end_frame();
        
    }
    glazy::destroy();
    Instrumentor::Get().EndSession();

}