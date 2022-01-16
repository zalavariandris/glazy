// MiniViewer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// std libraries
#include <iostream>
#include <set>
#include <cassert>
#include <filesystem>

// std data structures
#include <vector>
#include <string>

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"

// OpenImageIO
#include "OpenImageIO/imageio.h""
#include "OpenImageIO/imagecache.h"

// utilities
#include "ChannelsTable.h"

// gui state variables
std::filesystem::path file_pattern{ "" };
int start_frame{ 0 };
int end_frame{ 10 };

int frame{ 0 };
bool is_playing{ false };

GLuint make_texture(std::filesystem::path filename, std::vector<ChannelKey> channel_keys = {})
{
    if (channel_keys.empty()) return 0;
    if (!std::filesystem::exists(filename)) return 0;
    //auto channel_keys = get_index_column(selected_df);
    // read header
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

    // read pixels
    float* data = (float*)malloc(w * h * nchannels * sizeof(float));
    image_cache->get_pixels(OIIO::ustring(filename.string()), std::get<0>(channel_keys[0]), 0, x, x + w, y, y + h, 0, 1, chbegin, chend, OIIO::TypeFloat, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, chbegin, chend);

    // create texture
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
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

    // free data from memory after uploaded to gpu
    free(data);
    return tex;
}

// getters
std::filesystem::path _current_filename; // depends on file_pattern and frame
void update_current_filename() {
    _current_filename = sequence_item(file_pattern, frame);
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
    for (auto [layer_name, view_group] : _channels_table_tree) {
        layers.push_back(layer_name);
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
    tex = make_texture(_current_filename, get_index_column(_current_channels_df));
};

// actions
void open(std::filesystem::path filepath) {
    if (!filepath.empty()) {
        auto [pattern, start, end] = scan_for_sequence(filepath);
        file_pattern = pattern;
        start_frame = start;
        end_frame = end;

        // keep frame within framerange
        if (frame < start_frame) frame = start_frame;
        if (frame > end_frame) frame = end_frame;

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

// event handlers
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

void on_frame_change() {
    update_current_filename();
    update_channels_table();
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
                if (ImGui::TreeNodeEx(layer.c_str(), node_flags)) {
                    for (auto [view, df] : view_groups) {
                        //ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                        if (ImGui::TreeNodeEx(view.c_str(), node_flags)) {
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
        }
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

    if (ImGui::CollapsingHeader("overall spec"))
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
    if (ImGui::Button(is_playing ? "pause" : "play")) {
        is_playing = !is_playing;
    }
    ImGui::Text("%d", start_frame); ImGui::SameLine();
    if (ImGui::SliderInt("##frame", &frame, start_frame, end_frame)) {
        on_frame_change();
    } ImGui::SameLine();
    ImGui::Text("%d", end_frame);
    ImGui::EndGroup();
}



void ShowMiniViewer(bool *p_open) {
    if (ImGui::Begin("Viewer", p_open, ImGuiWindowFlags_NoCollapse)) {
        // Toolbar
        ImGui::BeginGroup();
        {
            ImGui::SetNextItemWidth(120);
            if (ImGui::Combo("##layers", &current_layer, layers)) {
                on_layer_changed();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            if (ImGui::Combo("##views", &current_view, views)) {
                on_view_change();
            }

            ImGui::SameLine();

            for (auto chan : channels) {
                ImGui::Button(chan.c_str()); ImGui::SameLine();
            }; ImGui::NewLine();
        }
        ImGui::EndGroup(); // toolbar end

        // Viewport
        {
            static GLuint fbo;
            static GLuint color_attachment;
            static Camera camera({0,0,5}, {0,0,0});
            auto item_pos = ImGui::GetCursorPos();
            auto item_size = ImGui::GetContentRegionAvail();

            // on resize
            static ImVec2 display_size;
            if (display_size.x != item_size.x || display_size.y != item_size.y) {
                display_size = item_size;
                // update attachments
                glDeleteTextures(1, &color_attachment);
                color_attachment = imdraw::make_texture_float(item_size.x, item_size.y, NULL, GL_RGBA);
                // update fbo
                glDeleteFramebuffers(1, &fbo);
                fbo = imdraw::make_fbo(color_attachment);
                // update camera aspect
                camera.aspect = item_size.x / item_size.y;
            }

            // Control Camera
            ImGui::InvisibleButton("camera control", item_size);
            if (ImGui::IsItemActive()) {
                if (ImGui::IsMouseDragging(0) && !ImGui::GetIO().KeyMods)
                {
                    camera.pan(-ImGui::GetIO().MouseDelta.x / item_size.x, -ImGui::GetIO().MouseDelta.y / item_size.y);
                }

                if (ImGui::IsMouseDragging(0) && (ImGui::GetIO().KeyMods == (ImGuiKeyModFlags_Ctrl | ImGuiKeyModFlags_Alt)))
                {
                    camera.orbit(-ImGui::GetIO().MouseDelta.x * 0.006, -ImGui::GetIO().MouseDelta.y * 0.006);
                }
            }
            if (ImGui::IsItemHovered()) {
                if (ImGui::GetIO().MouseWheel != 0 && !ImGui::GetIO().KeyMods) {
                    const auto target_distance = camera.get_target_distance();
                    camera.dolly(-ImGui::GetIO().MouseWheel * target_distance * 0.2);
                }
            }

            ImGui::SetItemAllowOverlap();
            ImGui::SetCursorPos(item_pos);
            ImGui::Image((ImTextureID)color_attachment, item_size, ImVec2(0, 1), ImVec2(1, 0));

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glClearColor(0.2, 0.2, 0.2, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glViewport(0, 0, item_size.x, item_size.y);
            imdraw::set_projection(camera.getProjection());
            imdraw::set_view(camera.getView());

            // draw scene
            imdraw::grid();
            imdraw::axis();
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
                // draw textured quad at ROI
                imdraw::quad(tex,
                    { spec.x*0.001, spec.y*0.001 },
                    { spec.x*0.001+spec.width*0.001, spec.y*0.001+spec.height*0.001}
                );

                // draw image full boundaries
                imdraw::rect({ spec.full_x*0.001,spec.full_y*0.001 }, { spec.full_width*0.001, spec.full_height*0.001 });
            }
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
}

int main()
{
    static bool image_viewer_visible{ true };
    static bool image_info_visible{ true };
    static bool channels_table_visible{ true };
    static bool timeline_visible{ true };
    glazy::init();
    while (glazy::is_running())
    {
        // control playback
        if (is_playing) {
            frame++;
            if (frame > end_frame) {
                frame = start_frame;
            }
            on_frame_change();
        }

        // GUI
        glazy::new_frame();

        // Main menubar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("file"))
            {
                if (ImGui::MenuItem("open")) {
                    auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0");
                    open(filepath);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("windows"))
            {
                ImGui::MenuItem("image viewer", "", &image_viewer_visible);
                ImGui::MenuItem("image info", "", &image_info_visible);
                ImGui::MenuItem("image channels table", "", &channels_table_visible);
                ImGui::EndMenu();
            }
            ImGui::Spacing();
            ImGui::Text("[%s]", file_pattern.string().c_str());
            
            ImGui::EndMainMenuBar();
        }
        // Image Viewer
        if (image_viewer_visible) {
            ShowMiniViewer(&image_viewer_visible);
        }
        // ChannelsTable
        if (channels_table_visible && ImGui::Begin("channels table", &channels_table_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
            ShowChannelsTable();
        }

        // Image info
        ImGui::SetNextWindowSizeConstraints(ImVec2(100, -1), ImVec2(200, -1));          // Width 400-500
        if (image_info_visible && ImGui::Begin("Info", &image_info_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
            ShowImageInfo();
        }

        // Timeliune
        if (timeline_visible && ImGui::Begin("timeline")) {
            ShowTimeline();
        }
        glazy::end_frame();
    }
    glazy::destroy();

}