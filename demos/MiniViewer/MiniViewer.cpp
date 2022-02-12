// MiniViewer.cpp
//

// std libraries
#include <iostream>
#include <set>
#include <cassert>
#include <filesystem>
#include <chrono>
#include <array>

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

// ImGui
#include "imgui.h"
#include "imgui_stdlib.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h" // use imgui math operators
// widgets
#include "imgui_widget_flamegraph.h"

// utilities
#include "ChannelsTable.h"
#include "ShaderToy.h"

// helpers
#include "helpers.h"


#define TRACY_CALLSTACK 5
#include <../tracy/Tracy.hpp>


// GUI State variables
struct State {
    std::filesystem::path file_pattern{ "" };
    int start_frame{ 0 };
    int end_frame{ 10 };

    int current_frame{ 0 };
    bool is_playing{ false };

    Camera camera = Camera({ 0,0,5000 }, { 0,0,0 }, { 0,1,0 });

    // computed
    std::filesystem::path _current_filename; // depends on file_pattern and frame
    ChannelsTable _channels_table; // depends on current filename
    std::vector<std::string> layers{}; // list of currently available layer names
    int current_layer{ 0 }; // currently selected layer index
    std::vector<std::string> views{}; // list of currently available view names
    int current_view{ 0 }; // currently selected view index
    ChannelsTable _current_channels_df;
    std::vector<std::string> channels{}; // list of currently available channel names
    OIIO::ImageSpec spec;
    GLuint tex{ 0 };
};
State state = State();

#pragma region GETTERS

auto get_image_cache() {
    auto image_cache = OIIO::ImageCache::create(true);
    return image_cache;
}

auto get_current_filename(const std::filesystem::path& file_pattern, int current_frame)
{
    ZoneScoped;
    return sequence_item(file_pattern, current_frame);
}

/// Read ChannelsTable from an exr file
///  test file formats: exr1, exr2, dpx, jpg
///  test single part singleview
///  test single part multiview
///  test multipart singleview
///  test multipart multiview
ChannelsTable get_channelstable(const std::filesystem::path& filename)
{
    ZoneScoped;
    if (!std::filesystem::exists(filename)) return {};

    // create a dataframe from all available channels
    // subimage <name,channelname> -> <layer,view,channel>
    ChannelsTable layers_dataframe;
    
    auto image_cache = get_image_cache();
    int nsubimages;
    {
        ZoneScopedN("get image info");
        static auto u_subimages = OIIO::ustring("subimages");
        image_cache->get_image_info(OIIO::ustring(filename.string().c_str()), 0, 0, u_subimages, OIIO::TypeInt, &nsubimages);
    }

    std::vector<std::string> views;
    {
        ZoneScopedN("get views");
        OIIO::ImageSpec spec;
        image_cache->get_imagespec(OIIO::ustring(filename.string().c_str()), spec, 0, 0);
        views = get_stringvector_attribute(spec, "multiView");
    }

    for (int p = 0; p < nsubimages; p++)
    {
        ZoneScopedN("get channels for single part");
        OIIO::ImageSpec spec;
        image_cache->get_imagespec(OIIO::ustring(filename.string().c_str()), spec, p, 0);
        std::string subimage_name = spec.get_string_attribute("name", "");
        std::string subimage_view = spec.get_string_attribute("view", "");
        if (ends_with(subimage_name, subimage_view))
        {
            subimage_name = subimage_name.substr(0, subimage_name.size() - subimage_view.size());
            trim(subimage_name, ".");
        }

        for (auto c = 0; c < spec.nchannels; c++)
        {
            if (nsubimages > 1)
            { // multipart
                std::tuple<int, int> key{ p, c };
                layers_dataframe.insert({ key, {subimage_name, subimage_view, subimage_name, subimage_view, spec.channel_name(c)} });
            }
            else
            {
                // singlepart
                auto [layer, view, channel] = parse_channel_name(spec.channel_name(c), views);
                std::tuple<int, int> key{ p, c };
                layers_dataframe.insert({ key, {subimage_name, subimage_view, layer, view, channel} });
            }
        }
    }
    return layers_dataframe;
}

auto get_layers(const ChannelsTable& channels_table)
{
    ZoneScoped;
    std::vector<std::string> layers;
    std::unordered_set<std::string> visited;
    for (const auto& [idx, record] : channels_table)
    {
        const auto& [subimage_idx, channel_idx] = idx;
        auto [subimage_name, subimage_view, layer, view, channel] = record;

        if (!visited.contains(layer)) {
            layers.push_back(layer);
            visited.insert(layer);
        }
    }
    return layers;
}

auto get_views(const ChannelsTable& channels_table, const std::vector<std::string>& layers, int current_layer)
{
    ZoneScoped;
    std::vector<std::string> views;
    std::unordered_set<std::string> visited;
    for (const auto& [idx, record] : channels_table) {
        const auto& [subimage_name, subimage_view, layer, view, channel] = record;
        if (!visited.contains(view))
        {
            if (layer == layers[current_layer]) {
                views.push_back(view);
                visited.insert(view);
            }
        }
    }
    return views;
}

ChannelsTable get_current_chanels_df(const ChannelsTable& channels_table, const std::vector<std::string>& layers, int current_layer, const std::vector<std::string>& views, int current_view) {
    ZoneScoped;
    ChannelsTable current_channels_df; // clear current channels table
    for (const auto& [idx, record] : channels_table) {
        const auto& [subimage_name, subimage_view, layer, view, channel] = record;
        if (layer == layers[current_layer] && view == views[current_view])
        {
            current_channels_df[idx] = record;
        }
    }

    return current_channels_df;
}

std::vector<std::string> get_channels(const ChannelsTable& current_channels_df)
{
    ZoneScoped;
    return get_channels_column(current_channels_df);
}

OIIO::ImageSpec get_spec(const std::filesystem::path& current_filename, const ChannelsTable& current_channels_df)
{
    ZoneScoped;
    if (!std::filesystem::exists(current_filename)) return OIIO::ImageSpec();
    if (current_channels_df.empty()) return OIIO::ImageSpec();

    OIIO::ImageSpec spec;
    auto channel_keys = get_index_column(current_channels_df);
    auto image_cache = get_image_cache();
    auto current_subimage = std::get<0>(channel_keys[0]);
    image_cache->get_imagespec(OIIO::ustring(current_filename.string()), spec, current_subimage, 0);
    return spec;
}

GLuint get_texture(const std::filesystem::path& current_filename, const ChannelsTable& current_channels_df)
{
    ZoneScopedS(6);
    auto indices = get_index_column(current_channels_df);
    if (indices.size() > 4) {
        indices.erase(indices.begin()+ 4, indices.end());
    }
    return make_texture_from_file(get_image_cache(), current_filename, indices);
}

#pragma endregion GETTERS

#pragma region ACTIONS
void open(const std::filesystem::path& filepath, bool sequence=false)
{
    if (!filepath.empty())
    {
        auto [pattern, start, end, selected_frame] = scan_for_sequence(filepath);
        state.file_pattern = pattern;
        state.start_frame = start;
        state.end_frame = end;
        state.current_frame = selected_frame;
        
        // keep frame within framerange
        if (state.current_frame < state.start_frame) state.current_frame = state.start_frame;
        if (state.current_frame > state.end_frame) state.current_frame = state.end_frame;

        // reset selection
        state.current_layer = 0; // reset to first layer
        state.current_view = 0; // reset current view

        // update computed state
        state._current_filename = get_current_filename(state.file_pattern, state.current_frame);
        state._channels_table = get_channelstable(state._current_filename);
        state.layers = get_layers(state._channels_table);
        state.views = get_views(state._channels_table, state.layers, state.current_layer);
        state._current_channels_df = get_current_chanels_df(state._channels_table, state.layers, state.current_layer, state.views, state.current_view);
        state.spec = get_spec(state._current_filename, state._current_channels_df);
        state.channels = get_channels(state._current_channels_df);
        if (glIsTexture(state.tex)) glDeleteTextures(1, &state.tex);
        state.tex = get_texture(state._current_filename, state._current_channels_df);
    }
};

void fit()
{
    float camDistance = std::max(state.spec.full_width / 2 / tan(state.camera.fovx() / 2), state.spec.full_height / 2 / tan(state.camera.fovy / 2));
    state.camera.eye = { state.spec.full_width / 2.0, state.spec.full_height / 2.0, camDistance };
    state.camera.target = { state.spec.full_width / 2.0, state.spec.full_height / 2.0, 0.0 };
}
#pragma endregion ACTIONS

#pragma region EVENT HANDLERS
void on_frame_change()
{
    ZoneScoped;
    state._current_filename = get_current_filename(state.file_pattern, state.current_frame);
    state._channels_table = get_channelstable(state._current_filename);
    state.layers = get_layers(state._channels_table);
    state.views = get_views(state._channels_table, state.layers, state.current_layer);
    state._current_channels_df = get_current_chanels_df(state._channels_table, state.layers, state.current_layer, state.views, state.current_view);
    state.spec = get_spec(state._current_filename, state._current_channels_df);
    state.channels = get_channels(state._current_channels_df);
    {
        ZoneScopedN("delete texture");
        if (glIsTexture(state.tex)) glDeleteTextures(1, &state.tex);
    }
    state.tex = get_texture(state._current_filename, state._current_channels_df);
};

void on_layer_change()
{
    ZoneScoped;
    state.views = get_views(state._channels_table, state.layers, state.current_layer);
    state.current_view = 0;
    state._current_channels_df = get_current_chanels_df(state._channels_table, state.layers, state.current_layer, state.views, state.current_view);
    state.spec = get_spec(state._current_filename, state._current_channels_df);
    state.channels = get_channels(state._current_channels_df);
    if (glIsTexture(state.tex)) glDeleteTextures(1, &state.tex);
    state.tex = get_texture(state._current_filename, state._current_channels_df);
};

void on_view_change()
{
    ZoneScoped;
    state._current_channels_df = get_current_chanels_df(state._channels_table, state.layers, state.current_layer, state.views, state.current_view);
    state.spec = get_spec(state._current_filename, state._current_channels_df);
    state.channels = get_channels(state._current_channels_df);
    if (glIsTexture(state.tex)) glDeleteTextures(1, &state.tex);
    state.tex = get_texture(state._current_filename, state._current_channels_df);
};
#pragma endregion EVENT HANDLERS

#pragma region WINDOWS
void ShowChannelsTable()
{
    ZoneScoped;
    if (ImGui::BeginTabBar("image header"))
    {
        if (ImGui::BeginTabItem("all channels"))
        {
            if (ImGui::BeginTable("channels table", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("index");
                ImGui::TableSetupColumn("subimage name");
                ImGui::TableSetupColumn("subimage view");
                ImGui::TableSetupColumn("layer");
                ImGui::TableSetupColumn("view");
                ImGui::TableSetupColumn("channel");
                ImGui::TableHeadersRow();
                for (const auto& [subimage_chan, layer_view_channel] : state._channels_table) {
                    auto [subimage, chan] = subimage_chan;
                    auto [subimage_name, subimage_view, layer, view, channel] = layer_view_channel;

                    //ImGui::TableNextRow();
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%d/%d", subimage, chan);

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", subimage_name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", subimage_view.c_str());

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
            for (auto [layer, view_groups] : get_channels_table_tree(state._channels_table)) {
                if (ImGui::TreeNodeEx(layer.c_str(), node_flags))
                {
                    for (auto [view, df] : view_groups) {
                        //ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                        
                        if (ImGui::TreeNodeEx(view.c_str(), node_flags))
                        {
                            for (auto [index, record] : df){
                                auto [subimage_name, subimage_view, layer, view, channel] = record;
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

void ShowInfo()
{
    ZoneScoped;
    if (ImGui::CollapsingHeader("sequence", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { ImGui::GetStyle().CellPadding.x,0 });
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvailWidth());
        if (ImGui::BeginTable("sequence table", 2))
        {
            ImGui::TableNextColumn();
            ImGui::Text("sequence:"); ImGui::TableNextColumn();
            ImGui::Text("%s", state.file_pattern.filename().string().c_str());\
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", state.file_pattern.parent_path().string().c_str());

            ImGui::TableNextColumn();
            ImGui::Text("range:"); ImGui::TableNextColumn();
            ImGui::Text("%d-%d", state.start_frame, state.end_frame);

            ImGui::TableNextColumn();
            ImGui::Text("sequence item:"); ImGui::TableNextColumn();
            ImGui::Text("%s", state._current_filename.filename().string().c_str());

            ImGui::EndTable();
        }
        ImGui::PopTextWrapPos();
        ImGui::PopStyleVar();
    }

    if (!std::filesystem::exists(state._current_filename)) return;

    auto in = OIIO::ImageInput::open(state._current_filename.string());
    if (ImGui::CollapsingHeader("file", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { ImGui::GetStyle().CellPadding.x,0 });
        if (ImGui::BeginTable("attributes", 2, ImGuiTableFlags_NoBordersInBody))
        {
            ImGui::TableNextColumn();
            ImGui::Text("valid file:");
            ImGui::TableNextColumn();
            ImGui::Text("%s", in->valid_file(state._current_filename.string()) ? "true" : "false");

            ImGui::TableNextColumn();
            ImGui::Text("format name:");
            ImGui::TableNextColumn();
            ImGui::Text(in->format_name());

            ImGui::EndTable();
        }

        if (ImGui::TreeNode("features")) {
            if (ImGui::BeginTable("features", 2, ImGuiTableFlags_NoBordersInBody))
            {
                for (auto feature : { "arbitrary_metadata","exif","ioproxy","iptc","procedural","thumbnail" })
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("%s:", feature);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", in->supports(OIIO::string_view(feature)));
                    ImGui::TableNextRow();
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        ImGui::PopStyleVar();
    }
    
    if (ImGui::CollapsingHeader("subimages", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTabBar("subimage"))
        {
            if (ImGui::BeginTabItem("channels"))
            {
                int nsubimages = 0;
                while (in->seek_subimage(nsubimages, 0))
                {
                    auto spec = in->spec();
                    if (ImGui::TreeNodeEx(std::to_string(nsubimages).c_str(), ImGuiTreeNodeFlags_DefaultOpen, "#%d %s", nsubimages, spec.get_string_attribute("name").c_str()))
                    {
                        for (auto i = 0; i < spec.channelnames.size(); i++)
                        {
                            auto text = spec.channelnames[i];
                            if (spec.channelformats.size() > i) {
                                text = text + " (" + (spec.channelformats[i].c_str()) + ")";
                            }

                            if (ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(text.c_str()).x > ImGui::GetContentRegionAvailWidth()) ImGui::NewLine();

                            ImGui::Text("%s", text.c_str());

                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                        ImGui::TreePop();
                    }
                    nsubimages++;
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("details"))
            {
                int nsubimages = 0;
                while (in->seek_subimage(nsubimages, 0))
                {
                    auto spec = in->spec();
                    if (ImGui::TreeNodeEx(std::to_string(nsubimages).c_str(), ImGuiTreeNodeFlags_DefaultOpen, "#%d %s", nsubimages, spec.get_string_attribute("name").c_str()))
                    {
                        std::string info = state.spec.serialize(OIIO::ImageSpec::SerialText, OIIO::ImageSpec::SerialDetailedHuman);
                        ImGui::TextWrapped("%s", info.c_str());
                        ImGui::TreePop();
                    }
                    
                    nsubimages++;
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    in->close();
}

void ShowTimeline()
{
    ZoneScoped;
    // Timeslider
    ImGui::BeginGroup();
    {
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_FAST_BACKWARD "##first frame")) {
            state.current_frame = state.start_frame;
            on_frame_change();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_STEP_BACKWARD "##step backward")) {
            state.current_frame -= 1;
            if (state.current_frame < state.start_frame) state.current_frame = state.end_frame;
            on_frame_change();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(state.is_playing ? ICON_FA_PAUSE : ICON_FA_PLAY "##play")) {
            state.is_playing = !state.is_playing;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_STEP_FORWARD "##step forward")) {
            state.current_frame += 1;
            if (state.current_frame > state.end_frame) state.current_frame = state.start_frame;
            on_frame_change();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_FAST_FORWARD "##last frame")) {
            state.current_frame = state.end_frame;
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
        ImGui::InputInt("##start frame", &state.start_frame, 0, 0);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::GetTextLineHeight() * 2 - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SliderInt("##frame", &state.current_frame, state.start_frame, state.end_frame)) {
            on_frame_change();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 2);
        ImGui::InputInt("##end frame", &state.end_frame, 0, 0);
        ImGui::EndDisabled();
    }
    ImGui::EndGroup();
}

namespace ImGui {
    int CalcComboWidth(const std::vector<std::string>& values, ImGuiComboFlags flags=0)
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
        if ((flags & ImGuiComboFlags_NoArrowButton) == 0) min_width += ImGui::GetTextLineHeight()+ImGui::GetStyle().FramePadding.y*2;
        return min_width;
    }
}

void ShowMiniViewer(bool *p_open)
{
    ZoneScoped;
    static float gain = 0.0;
    static float gamma = 1.0;
    static bool display_checkerboard=true;
    static glm::vec4 pipett_color;
    static bool flip_y{ false };

    static int selected_device = 1;

    if (ImGui::Begin("Viewer", p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
    {

        if (ImGui::BeginMenuBar()) // Toolbar
        {

            ImGui::BeginGroup(); // Channel secetion group
            {
                ImGui::SetNextItemWidth(ImGui::CalcComboWidth(state.layers, ImGuiComboFlags_NoArrowButton));
                if (ImGui::BeginCombo("##layers", state.layers.size()>0 ? state.layers[state.current_layer].c_str() : "-", ImGuiComboFlags_NoArrowButton))
                {
                    for (auto i = 0; i < state.layers.size(); i++) {
                        const bool is_selected = state.layers[i] == state.layers[state.current_layer];
                        if (ImGui::Selectable(state.layers[i].c_str(), is_selected)) {
                            state.current_layer = i;
                            on_layer_change();
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("layers");

                //ImGui::SameLine();

                if (state.views.size() == 1)
                {
                    if (!state.views[state.current_view].empty())
                    {
                        ImGui::Text(state.views[state.current_view].c_str());
                    }
                    else {
                    }
                }
                else if (state.views.size() > 1)
                {
                    ImGui::SetNextItemWidth(ImGui::CalcComboWidth(state.views));
                    if (ImGui::Combo("##views", &state.current_view, state.views)) {
                        on_view_change();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("views");
                }

                //ImGui::SameLine();

                for (auto chan : state.channels) {
                    ImGui::MenuItem(chan.c_str());
                };
            }
            ImGui::EndGroup(); // toolbar end

            // center align
            auto style = ImGui::GetStyle();
            float center_group_width = ImGui::CalcTextSize("fit").x + ImGui::CalcTextSize("flip y").x + ImGui::CalcTextSize(ICON_FA_CHESS_BOARD).x + style.ItemSpacing.x * 2 + style.FramePadding.x * 4;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x/2 - center_group_width/2.0);
            ImGui::BeginGroup();
            {
                if (ImGui::Button("fit")) {
                    fit();
                }
                if (ImGui::Button("flip y")) {
                    state.camera = Camera(
                        state.camera.eye * glm::vec3(1, 1, -1),
                        state.camera.target * glm::vec3(1, 1, -1),
                        state.camera.up * glm::vec3(1, -1, 1),
                        state.camera.ortho,
                        state.camera.aspect,
                        state.camera.fovy,
                        state.camera.tiltshift,
                        state.camera.near_plane,
                        state.camera.far_plane
                    );
                }
                ImGui::MenuItem(ICON_FA_CHESS_BOARD, "", &display_checkerboard);
            }
            ImGui::EndGroup();

            // right align
            static const std::vector<std::string> devices{ "linear", "sRGB", "Rec.709" };
            int devices_combo_width = ImGui::CalcComboWidth(devices);
            
            auto right_group_width = ImGui::GetTextLineHeight() * 6*2 + ImGui::CalcTextSize(ICON_FA_ADJUST).x + style.ItemInnerSpacing.x + style.ItemSpacing.x + style.ItemSpacing.x*2 + devices_combo_width;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - right_group_width);
            ImGui::BeginGroup(); // display correction
            {
                ImGui::SetNextItemWidth(ImGui::GetTextLineHeight()*6);
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
        ImGui::EndMenuBar();
        }

        // Viewport
        {
            static GLuint viewport_fbo;
            static GLuint viewport_color_attachment;
            static glm::ivec2 viewport_size;

            if (ImGui::BeginChild("viewport-viewer", ImGui::GetContentRegionAvail()-ImVec2(0, ImGui::GetFrameHeight()), false, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
            {
                auto item_size = ImGui::GetContentRegionAvail();
                auto item_pos = ImGui::GetCursorPos(); // if child window has no border this is: 0,0

                // Resize Viewport FBO
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
                }

                TracyPlot("ViewportSize", (int64_t)(viewport_size.x*viewport_size.y));

                // Control Camera
                ImGui::InvisibleButton("camera control", item_size);
                if (ImGui::IsItemActive()) {
                    if (ImGui::IsMouseDragging(0) && (ImGui::GetIO().KeyMods == (ImGuiKeyModFlags_Ctrl | ImGuiKeyModFlags_Alt)))
                    {
                        state.camera.orbit(-ImGui::GetIO().MouseDelta.x * 0.006, -ImGui::GetIO().MouseDelta.y * 0.006);
                    }
                    else if (ImGui::IsMouseDragging(0))// && !ImGui::GetIO().KeyMods)
                    {
                        state.camera.pan(-ImGui::GetIO().MouseDelta.x / item_size.x, -ImGui::GetIO().MouseDelta.y / item_size.y);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    if (ImGui::GetIO().MouseWheel != 0 && !ImGui::GetIO().KeyMods) {
                        const auto target_distance = state.camera.get_target_distance();
                        state.camera.dolly(-ImGui::GetIO().MouseWheel * target_distance * 0.2);
                    }
                }
                // Display viewport GUI
                ImGui::SetCursorPos(item_pos);
                ImGui::SetItemAllowOverlap();
                ImGui::Image((ImTextureID)viewport_color_attachment, item_size, ImVec2(0, 1), ImVec2(1, 0));

                if (!state.spec.undefined())
                {
                    glm::vec3 screen_pos = glm::project(glm::vec3(state.spec.full_width, state.spec.full_height, 0.0), state.camera.getView(), state.camera.getProjection(), glm::vec4(0, 0, item_size.x, item_size.y));
                    screen_pos.y = item_size.y - screen_pos.y; // invert y
                    ImGui::SetCursorPos(ImVec2(screen_pos.x + item_pos.x, screen_pos.y + item_pos.y));
                    if (state.spec.full_width == 1920 && state.spec.full_height == 1080)
                    {
                        ImGui::Text("HD");
                    }
                    else if (state.spec.full_width == 3840 && state.spec.full_height == 2160)
                    {
                        ImGui::Text("UHD 4K");
                    }
                    else if (state.spec.full_width == state.spec.full_height)
                    {
                        ImGui::Text("Square %d", state.spec.full_width);
                    }
                    else
                    {
                        ImGui::Text("%dx%d", state.spec.full_width, state.spec.full_height);
                    }
                }
                
                ImGui::SetCursorPos(item_pos);

                // Make Image Correction texture
                static GLuint correction_fbo;
                static GLuint correction_color_attachment;
                static glm::ivec2 correction_size;

                if (!state.spec.undefined()) {
                    if (correction_size.x != state.spec.full_width || correction_size.y != state.spec.full_height)
                    {
                        ZoneScopedN("update correction fbo");
                        std::cout << "update correction fbo: " << state.spec.full_width << ", " << state.spec.full_height << "\n";
                        correction_size = { state.spec.full_width, state.spec.full_height };
                        if (glIsFramebuffer(correction_fbo))
                            glDeleteFramebuffers(1, &correction_fbo);
                        if (glIsTexture(correction_color_attachment))
                            glDeleteTextures(1, &correction_color_attachment);
                        correction_color_attachment = imdraw::make_texture_float(correction_size.x, correction_size.y, NULL, GL_RGBA);
                        correction_fbo = imdraw::make_fbo(correction_color_attachment);
                    }
                }

                const char* PASS_THROUGH_VERTEX_CODE = R"(
                    #version 330 core
                    layout (location = 0) in vec3 aPos;
                    void main()
                    {
                        gl_Position = vec4(aPos, 1.0);
                    }
                    )";

                static GLuint correction_program;
                if (glazy::is_file_modified("display_correction.frag"))
                {
                    ZoneScopedN("recompile display correction shader");
                    if (glIsProgram(correction_program)) glDeleteProgram(correction_program);
                    std::string correction_fragment_code = glazy::read_text("display_correction.frag");
                    correction_program = imdraw::make_program_from_source(PASS_THROUGH_VERTEX_CODE, correction_fragment_code.c_str());
                }

                // render to correction fbo
                BeginRenderToTexture(correction_fbo, 0, 0, state.spec.full_width, state.spec.full_height);
                {
                    ZoneScopedN("render to correction fbo");
                    glClearColor(0, 0, 0, 0.0);
                    glClear(GL_COLOR_BUFFER_BIT);

                    imdraw::push_program(correction_program);
                    /// Draw quad with fragment shader
                    imgeo::quad();
                    static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
                    static auto vao = imdraw::make_vao(correction_program, { {"aPos", {vbo, 3}} });

                    imdraw::set_uniforms(correction_program, {
                        {"inputTexture", 0},
                        {"resolution", glm::vec2(state.spec.full_width, state.spec.full_height)},
                        {"gain_correction", gain},
                        {"gamma_correction", 1.0f/gamma},
                        {"convert_to_device", selected_device},
                        });
                    glBindTexture(GL_TEXTURE_2D, state.tex);
                    glBindVertexArray(vao);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                    glBindVertexArray(0);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    imdraw::pop_program();
                }
                EndRenderToTexture();

                // Render to Viewport FBO
                BeginRenderToTexture(viewport_fbo, 0, 0, item_size.x, item_size.y);
                {
                    ZoneScopedN("render to viewport fbo");
                    glClearColor(0.0, 0.0, 0.0, 0.1);
                    glClear(GL_COLOR_BUFFER_BIT);
                    state.camera.aspect = item_size.x / item_size.y;
                    imdraw::set_projection(state.camera.getProjection());
                    imdraw::set_view(state.camera.getView());

                    /// Draw scene
                    // draw background
                    {
                        ZoneScopedN("draw background");
                        static std::filesystem::path fragment_path{ "./polka.frag" };
                        static std::string fragment_code;
                        static GLuint polka_program;
                        if (glazy::is_file_modified(fragment_path)) {
                            // reload shader
                            ZoneScopedN("recompile polka shader");
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
                            {"projection", state.camera.getProjection()},
                            {"view", state.camera.getView()},
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
                    if (!state._current_channels_df.empty()) {
                        ZoneScopedN("draw image");
                        // read header
                        //auto image_cache = OIIO::ImageCache::create(true);
                        //OIIO::ImageSpec spec;
                        //auto channel_keys = get_index_column(state._current_channels_df);
                        //int SUBIMAGE = std::get<0>(channel_keys[0]); //todo: ALL SUBIMAGE MUST MATCH
                        glm::vec2 min_rect{ state.spec.full_x, state.spec.full_y };
                        glm::vec2 max_rect{ state.spec.full_x + state.spec.full_width, state.spec.full_y + state.spec.full_height };

                        // draw checkerboard or black background
                        if (display_checkerboard)
                        { // draw checkerboard background
                            ZoneScopedN("draw checkerboard background");
                            glDisable(GL_BLEND);
                            static auto toy = ShaderToy("./try_shadertoy.frag", imgeo::rect({ 0,0 }, {1,1}));
                            toy.autoreload();

                            glm::mat4 M{1};
                            auto size = max_rect - min_rect;

                            M = glm::translate(M, { min_rect.x, min_rect.y, 0 });

                            toy.draw({
                                { "uResolution", glm::ivec2(state.spec.full_width, state.spec.full_height) },
                                { "projection", state.camera.getProjection() },
                                { "view", state.camera.getView() },
                                { "model", M}
                            });
                            glEnable(GL_BLEND);
                        }
                        else
                        { // draw black background
                            ZoneScopedN("draw black background");
                            imdraw::rect(
                                { state.spec.full_x, state.spec.full_y }, // min rect
                                { state.spec.full_x + state.spec.full_width, state.spec.full_y + state.spec.full_height }, //max rect
                            { //Material
                                .color = glm::vec3(0,0,0)
                            }
                            );
                        }

                        {
                            ZoneScopedN("draw image quad");
                            // draw textured quad
                            static float image_opacity;
                            ImGui::SliderFloat("image opacity", &image_opacity, 0.0f, 1.0f);
                            imdraw::quad(correction_color_attachment,
                                min_rect,
                                max_rect,
                                glm::vec2(1, 1), glm::vec2(0, 0), image_opacity
                            );
                        }

                        /// Draw boundaries
                        // full image boundaries
                        imdraw::rect(
                            min_rect,
                            max_rect,
                            { .mode = imdraw::LINE, .color = glm::vec3(0.5), .opacity = 0.3 }
                        );

                        // data window boundaries
                        imdraw::rect(
                            { state.spec.x, state.spec.y },
                            { state.spec.x + state.spec.width, state.spec.y + state.spec.height },
                            { .mode = imdraw::LINE, .color = glm::vec3(0.5), .opacity = 0.3 }
                        );
                    }

                    //imdraw::grid();
                    imdraw::axis(100);

                    ImVec2 mousepos = ImGui::GetMousePos() - ImGui::GetWindowPos();
                    auto worldpos = state.camera.screen_to_planeXY(mousepos.x, ImGui::GetWindowSize().y-mousepos.y, { 0,0, item_size.x, item_size.y });
                    glm::ivec2 pixelpos = { floor(worldpos.x), floor(worldpos.y) };
                    imdraw::rect({ pixelpos.x, pixelpos.y }, { pixelpos.x + 1, pixelpos.y + 1 });

                    //if (!state.spec.undefined()) {
                    //    int x = pixelpos.x;
                    //    int y = pixelpos.y;
                    //    int w = 1;
                    //    int h = 1;
                    //    auto channel_keys = get_index_column(state._current_channels_df);
                    //    int chbegin = std::get<1>(channel_keys[0]);
                    //    int chend = std::get<1>(channel_keys[channel_keys.size() - 1]) + 1; // channel range is exclusive [0-3)
                    //    int nchannels = chend - chbegin;
                    //    
                    //    auto image_cache = OIIO::ImageCache::create(true);
                    //    float* data = (float*)malloc(w * h * nchannels * sizeof(float));
                    //    image_cache->get_pixels(OIIO::ustring(state._current_filename.string()), std::get<0>(channel_keys[0]), 0, x, x + w, y, y + h, 0, 1, chbegin, chend, OIIO::TypeFloat, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, chbegin, chend);

//    for (auto i = 0; i < nchannels && i<4; i++)
//    {
//        pipett_color[i] = data[i];
//    }
//    free(data);
//}
                }
                EndRenderToTexture();
            }
            ImGui::EndChild();

            {
                ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground |
                    ImGuiWindowFlags_MenuBar;

                if (ImGui::BeginChild("viewport-sidebar_down", ImGui::GetContentRegionAvail(), false, flags))
                {
                    if (ImGui::BeginMenuBar())
                    {
                        ImGui::ColorButton("pipett", { pipett_color[0], pipett_color[1], pipett_color[2], pipett_color[3] });
                        ImGui::SameLine();
                        ImGui::Text("%.2f %.2f %.2f %.2f", pipett_color[0], pipett_color[1], pipett_color[2], pipett_color[3]);
                        ImGui::EndMenuBar();
                    }
                }
                ImGui::EndChild();
            }
        }
    }
    ImGui::End(); // End Viewer window
}

void ShowSettings(bool* p_open)
{

    if (ImGui::Begin("Settings", p_open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::CollapsingHeader("GLFW", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static bool vsync = true;
            if (ImGui::Checkbox("vsync", &vsync)) {
                glfwSwapInterval(vsync ? 1 : 0);     // 0: vsync-off, 1: vsync-on
            }
        }

        if (ImGui::CollapsingHeader("OpenImageIO", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static int threads = 0;
            static int exr_threads = 0;
            static bool try_all_readers = false;
            static bool openexr_core = 1;
            static int autotile = 64;
            OIIO::getattribute("threads", threads);
            if (ImGui::DragInt("threads", &threads, 1.0f, 0, 32)) {
                OIIO::attribute("threads", threads);
            }

            OIIO::getattribute("exr_threads", exr_threads);
            if (ImGui::DragInt("exr_threads", &exr_threads, 1.0f, 0, 32)) {
                OIIO::attribute("exr_threads", exr_threads);
            }

            int got_try_all_readers;
            OIIO::getattribute("try_all_readers", got_try_all_readers);
            try_all_readers = got_try_all_readers > 0 ? true : false;
            if (ImGui::Checkbox("try all readers", &try_all_readers)) {
                OIIO::attribute("try_all_readers", try_all_readers ? 1 : 0);
            }

            int got_openexr_core;
            OIIO::getattribute("openexr:core", got_openexr_core);
            openexr_core = got_openexr_core > 0 ? true : false;
            if (ImGui::Checkbox("openexr:core", &openexr_core)) {
                OIIO::attribute("openexr:core", openexr_core ? 1 : 0);
            }

            OIIO::getattribute("autotile", autotile);
            if (ImGui::DragInt("autotile", &autotile, 1.0f, 16, 512)) {
                OIIO::attribute("autotile", autotile);
            }
        }
    }
    ImGui::End();
}

#pragma endregion WINDOWS

void ShowStatsWindow(bool* p_open) {
    if (ImGui::Begin("Stats", p_open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ZoneScoped("StatsWindow");

        auto image_cache = get_image_cache();

        if (ImGui::CollapsingHeader("OIIO:ImageCache", ImGuiTreeNodeFlags_DefaultOpen)) {

            if(ImGui::Button("invalidate"))
            {
                ZoneScoped("invalidate image cache");
                image_cache->invalidate_all(true);
            }

            int max_open_files;
            int autotile;
            int autoscanline;
            int automip;
            int forcefloat;

            image_cache->getattribute("max_open_files", max_open_files);
            image_cache->getattribute("autotile", autotile);
            image_cache->getattribute("autoscanline ", autoscanline);
            image_cache->getattribute("automip", automip);
            image_cache->getattribute("forcefloat", forcefloat);

            ImGui::Text("max open files: %d", max_open_files);
            ImGui::Text("autotile: %d", autotile);
            ImGui::Text("autoscanline: %d", autoscanline);
            ImGui::Text("automip: %d", automip);
            ImGui::Text("forcefloat: %d", forcefloat);

            int total_files;
            image_cache->getattribute("total_files", total_files);

            // stats
            int64_t cache_memory_used;
            int open_files_peak;
            int64_t image_size;
            int64_t file_size;
            int64_t bytes_read;
            image_cache->getattribute("stat:cache_memory_used", OIIO::TypeInt64, &cache_memory_used);
            image_cache->getattribute("stat:open_files_peak", open_files_peak);
            image_cache->getattribute("stat:image_size", OIIO::TypeInt64, &image_size);
            image_cache->getattribute("stat:file_size", OIIO::TypeInt64, &file_size);
            image_cache->getattribute("stat:bytes_read", OIIO::TypeInt64, &bytes_read);

            ImGui::Text("cache memory used: %d", cache_memory_used);
            ImGui::Text("open_files_peak: %d", open_files_peak);
            ImGui::Text("image_size: %d", image_size);
            ImGui::Text("file_size: %d", file_size);
            ImGui::Text("bytes_read: %d", bytes_read);
            ImGui::End();
        }
    }
}

int main()
{
    OIIO::attribute("threads", 8);
    OIIO::attribute("exr_threads", 8);
    OIIO::attribute("try_all_readers", 0);
    OIIO::attribute("openexr:core", 0);

    static bool viewer_visible{ true };
    static bool info_visible{ true };
    static bool channels_table_visible{ true };
    static bool timeline_visible{ true };
    static bool settings_visible{ true };
    static bool stats_visible{ true };

    auto image_cache = get_image_cache();
    image_cache->attribute("max_memory_MB", 1024.0f*16);
    //image_cache->attribute("autotile", 64);
    //image_cache->attribute("forcefloat", 1);
    //image_cache->attribute("max_open_files", 1);
    
    glazy::init();
    
    while (glazy::is_running())
    {
        ZoneScopedN("frame");
        // control playback
        if (state.is_playing)
        {
            state.current_frame++;
            if (state.current_frame > state.end_frame) {
                state.current_frame = state.start_frame;
            }
            on_frame_change();
        }

        // GUI
        glazy::new_frame();
        {
                
            // Main menubar
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open"))
                    {
                        auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg");
                        open(filepath);
                        fit();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View")) {
                    if (ImGui::MenuItem("Fit", "f")) {
                        fit();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Windows"))
                {
                    ImGui::MenuItem("viewer", "", &viewer_visible);
                    ImGui::MenuItem("info", "", &info_visible);
                    ImGui::MenuItem("channels table", "", &channels_table_visible);
                    ImGui::MenuItem("timeline", "", &timeline_visible);
                    ImGui::MenuItem("settings", "", &settings_visible);
                    ImGui::MenuItem("stats", "", &stats_visible);
                    ImGui::EndMenu();
                }
                ImGui::Spacing();

                if (!state.file_pattern.empty())
                {
                    ImGui::Text(" - %s", state.file_pattern.filename().string().c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(state.file_pattern.string().c_str());
                    }
                }

                ImGui::EndMainMenuBar();
            }
 
            // Image Viewer
            if (viewer_visible)
            {
                ShowMiniViewer(&viewer_visible);
            }

            // Image Viewer
            if (settings_visible)
            {
                ShowSettings(&settings_visible);
            }

            // Timeline
            if (timeline_visible)
            {
                if (ImGui::Begin("Timeline", &timeline_visible)) {
                    ShowTimeline();
                }
                ImGui::End();
            }

            // Statistics
            if (stats_visible) {
                ShowStatsWindow(&stats_visible);
            }
                
            // ChannelsTable
            if (channels_table_visible)
            {
                if (ImGui::Begin("Channels table", &channels_table_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
                    ShowChannelsTable();
                }
                ImGui::End();
            }

            // Image info
            ImGui::SetNextWindowSizeConstraints(ImVec2(100, -1), ImVec2(200, -1));          // Width 400-500
            if (info_visible)
            {
                if (ImGui::Begin("Info", &info_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
                    ShowInfo();
                }
                ImGui::End();
            }                
        }
            
        glazy::end_frame();

        FrameMark;
    }
    glazy::destroy();

}