// exrviewer2.cpp : This file contains the 'main' function. Program execution begins and ends there.
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
#include "Store.h"

std::set<GLuint> textures_to_delete;
void delete_texture_later(GLuint tex) {
    textures_to_delete.insert(tex);
}

/// Gui for ChannelsTable
bool ImGui_ChannelsTable(const ChannelsTable& channels_dataframe) {
    ImGui::BeginGroup();
    if (ImGui::BeginTable("channels table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendX |ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("subimage/chan");
        ImGui::TableSetupColumn("layer");
        ImGui::TableSetupColumn("view");
        ImGui::TableSetupColumn("channel");
        ImGui::TableHeadersRow();
        for (const auto& [subimage_chan, layer_view_channel] : channels_dataframe) {
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
    ImGui::EndGroup();
}

void ImGui_Display(const std::vector<int> &values) {
    ImGui::BeginGroup();
    for (auto val : values) {
        ImGui::Text("%d", val);
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::EndGroup();
}

bool ImGui_SanitycheckChannelstable(const ChannelsTable &df) {
    // check if channels are the same subimage
    std::vector<int> subimages = get_subimage_idx_column(df);

    bool AreTheSameSubmages = std::all_of(subimages.begin(), subimages.end(), [&](int i) {return i == subimages[0]; });
    // check if RGBA and in right order
    std::vector<std::string> channels = get_channels_column(df);

    // check channels count
    bool ChannelsCountWithinRange = channels.size() > 0 && channels.size() <= 4;

    // check if RGBA and in wrong order
    bool R_OK = indexOf(channels, "R") == -1 || indexOf(channels, "R") == 0;
    bool G_OK = indexOf(channels, "G") == -1 || indexOf(channels, "G") == 1;
    bool B_OK = indexOf(channels, "B") == -1 || indexOf(channels, "B") == 2;
    bool A_OK = indexOf(channels, "A") == -1 || indexOf(channels, "A") == 3 || indexOf(channels, "A") == 0;
    bool RGBA_ORDERED = R_OK && G_OK && B_OK && A_OK;

    // check if xyz and in right order
    //                         
    bool X_OK = indexOf(channels, "x") == -1 || indexOf(channels, "x") == 0;
    bool Y_OK = indexOf(channels, "y") == -1 || indexOf(channels, "y") == 1;
    bool Z_OK = indexOf(channels, "z") == -1 || indexOf(channels, "z") == 2 || indexOf(channels, "z") == 0;
    bool XYZ_ORDERED = X_OK && Y_OK && Z_OK;

    if (!ChannelsCountWithinRange) {
        ImGui::TextColored(ImColor(255, 255, 0), "channels count not within displayable range [1-4], got: %d", channels.size());
    }

    if (!AreTheSameSubmages) {
        ImGui::TextColored(ImColor(255, 255, 0), "Channels are from multiple subimages");
    }
    if (!RGBA_ORDERED) {
        ImGui::TextColored(ImColor(255, 255, 0), "RGBA in wrong order!");
    }
    if (!XYZ_ORDERED) {
        ImGui::TextColored(ImColor(255, 255, 0), "XYZ in wrong order!");
    }

    return AreTheSameSubmages && ChannelsCountWithinRange && RGBA_ORDERED && XYZ_ORDERED;
}

GLuint make_texture(std::filesystem::path filename, std::vector<ChannelKey> channel_keys={{0,0},{0,1},{0,2}}) {
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

void ImGui_DisplayImage(std::filesystem::path filename, std::vector<ChannelKey> channel_keys, const ImVec2 &size)
{
    using Key = std::tuple<std::filesystem::path, std::vector<ChannelKey>>;
    static std::map<Key, GLuint> cache;

    Key key{ filename, channel_keys };
    if (!cache.contains(key)) {
        
        cache[key] = make_texture(filename, channel_keys);
    }

    //delete_texture_later(cache[key]);
    ImGui::Image((ImTextureID)cache[key], size);
}

void TestImageSequence() {
    std::filesystem::path filename = "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr";

    ImGui::Text("filename: %s", filename.string().c_str());

    auto sequence = scan_for_sequence(filename);
    auto [pattern, start_frame, end_frame] = sequence;
    ImGui::Text("pattern: %s %d-%d", pattern.string().c_str(), start_frame, end_frame);

    static int frame;
    ImGui::SliderInt("frame", &frame, start_frame, end_frame);

    auto item = sequence_item(pattern, frame);
    ImGui::Text("item: %s", item.string().c_str());
}

void TestEXRLayers() {
    std::filesystem::path filename = "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr";

    /// Prepare data for display
    // Read channels dataframe
    ChannelsTable channels_table = get_channelstable(filename);
    static ChannelsTable selected_df;

    // Group by layers and views
    std::map<std::tuple<std::string, std::string>, ChannelsTable> layer_view_groups = group_by_layer_view(channels_table);
    std::map<std::string, ChannelsTable> layer_groups = group_by_layer(channels_table); // split channels to layers
    std::map<std::string, ChannelsTable> view_groups = group_by_view(channels_table);   // split channels to layers
    std::map<std::string, std::map<std::string, ChannelsTable>> layer_view_map; // split channels to layers with views
    for (const auto& [layer, channels] : group_by_layer(channels_table)) {
        layer_view_map[layer] = group_by_view(channels);
    }

    // Get Views
    auto image_cache = OIIO::ImageCache::create(true);
    OIIO::ImageSpec spec;
    image_cache->get_imagespec(OIIO::ustring(filename.string()), spec, 0, 0);
    auto views = get_stringvector_attribute(spec, "multiView");

    /// Display GUI

    // Show filename
    ImGui::Text("filename: %s", filename.string().c_str());
    ImGui::Separator();

    // display views
    ImGui::Text("views: "); ImGui::SameLine();
    for (auto view : views) {
        ImGui::Text(view.c_str()); ImGui::SameLine();
    } ImGui::NewLine();

    ImGui::Separator();

    // Show DataFrame
    if (ImGui::BeginChild("channels_column", {ImGui::GetContentRegionAvailWidth()-512, ImGui::GetContentRegionAvail().y})) {
        if (ImGui::BeginTabBar("image header")) {
            if (ImGui::BeginTabItem("channels dataframe")) {
                ImGui_ChannelsTable(channels_table);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("layer-view")) {
                for (const auto& [layer_view, df] : layer_view_groups) {
                    auto [layer, view] = layer_view;
                    ImGui_ChannelsTable(df);
                    if (ImGui::Selectable(("SHOW" + layer + "-" + view).c_str())) {
                        selected_df = df;
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("layer -> view"))
            {
                for (auto [layer, view_groups] : layer_view_map) {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (ImGui::TreeNode(layer.c_str())) {
                        for (auto [view, df] : view_groups) {
                            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                            if (ImGui::TreeNode(view.c_str())) {
                                if (ImGui::Selectable(layer.c_str())) {
                                    selected_df = df;
                                }
                                //ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth() - 232);
                                if (ImGui_SanitycheckChannelstable(df)) {
                                    ImGui_DisplayImage(filename, get_index_column(df), ImVec2(232, 232));
                                }
                                ImGui::SameLine();
                                ImGui_ChannelsTable(df);
                                ImGui::TreePop();
                            }
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndChild();
    }

    /*
        display image from selection
    */
    ImGui::SameLine();
    if(ImGui::BeginChild("viewer column", {512, ImGui::GetContentRegionAvail().y}))
    {       
        if (ImGui_SanitycheckChannelstable(selected_df))
        {
            ImGui_DisplayImage(filename, get_index_column(selected_df), ImVec2(512, 512));
        }
        ImGui::EndChild();
    }
}

template <class T>
void drop_duplicates(std::vector<T> &vec) {
    std::set<T> values;
    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const T& value) {
        return !values.insert(value).second;
    }), vec.end());
}

void ShowExrViewer() {
    // gui state
    static std::filesystem::path file_pattern{ "" };
    static int start_frame{0};
    static int end_frame{ 10 };

    static int frame{ 0 };
    static bool is_playing{ false };

    // getters
    static std::filesystem::path _current_filename; // depends on file_pattern and frame
    static auto update_current_filename = []() {
        _current_filename = sequence_item(file_pattern, frame);
    };

    static ChannelsTable _channels_table; // depends on current filename
    static auto update_channels_table = []() {
        _channels_table = get_channelstable(_current_filename);
    };

    static std::map<std::string, std::map<std::string, ChannelsTable>> _channels_table_tree;
    static auto update_channels_table_tree = []() {
        _channels_table_tree.clear();
        for (const auto& [layer, channels] : group_by_layer(_channels_table)) {
            _channels_table_tree[layer] = group_by_view(channels);
        }
    };

    static std::vector<std::string> layers{}; // list of currently available layer names
    static auto update_layers = []() {
        layers.clear();
        for (auto [layer_name, view_group] : _channels_table_tree) {
            layers.push_back(layer_name);
        }
    };
    static int current_layer{ 0 }; // currently selected layer index

    static std::vector<std::string> views{}; // list of currently available view names
    static auto update_views = []() {
        views.clear();
        for (auto [view_name, df] : _channels_table_tree[layers[current_layer]]) {
            views.push_back(view_name);
        }
    };
    static int current_view{ 0 }; // currently selected view index

    static ChannelsTable _current_channels_df;
    static auto update_current_channels_df = []() {
        _current_channels_df = _channels_table_tree[layers[current_layer]][views[current_view]];
    };

    static std::vector<std::string> channels{}; // list of currently available channel names
    static auto update_channels = []() {
        channels.clear();
        for (auto [index, record] : _current_channels_df) {
            auto [layer, view, channel_name] = record;
            channels.push_back(channel_name);
        }
    };

    static GLuint tex{ 0 };
    static auto update_texture = []() {
        if (glIsTexture(tex)) glDeleteTextures(1, &tex);
        tex = make_texture(_current_filename, get_index_column(_current_channels_df));
    };
    

    // actions
    auto open = [&](std::filesystem::path filepath) {
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

    auto on_layer_changed = [&]() {
        update_views();
        current_view = 0;
        update_current_channels_df();
        update_channels();
        update_texture();
    };

    auto on_view_change = []() {
        update_current_channels_df();
        update_channels();
        update_texture();
    };

    auto on_frame_change = []() {
        update_current_filename();
        update_channels_table();
        update_texture();
    };

    // control playback
    if (is_playing) {
        frame++;
        if (frame > end_frame) {
            frame = start_frame;
        }
        on_frame_change();
    }

    // Toolbar
    ImGui::BeginGroup();
    
    if (ImGui::Button(file_pattern.empty() ? "open" : file_pattern.string().c_str(), { 120,0 })) // file input
    {
        auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0");
        open(filepath);
    }
    if (!file_pattern.empty() && ImGui::IsItemHovered()) // file tooltip
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(file_pattern.string().c_str());
        ImGui::EndTooltip();
    }

    ImGui::SameLine();

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

    ImGui::EndGroup(); // toolbar end

    // Image
    ImGui::Image((ImTextureID)tex, { 512,512 });

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

/* state */
auto state_pattern = State<std::filesystem::path>("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/multipart.%04d.exr", "pattern");
auto state_frame = State<int>(1, "frame");
auto selected_group = State<std::string>("color", "selected\ngroup");
auto selected_subimage = State<int>(0, "selected\nsubimage");

// non reactive state
int START_FRAME = 1;
int END_FRAME = 8;

/* getters */
auto computed_input_frame = Computed<std::filesystem::path>([]() {
    std::cout << "evaluate input_frame" << "\n";
    return sequence_item(state_pattern.get(), state_frame.get());
}, "input frame");

auto subimages = Computed<std::vector<std::string>>([]() {
    std::vector<std::string> result;
    if(!std::filesystem::exists(computed_input_frame.get())) return result;

    std::cout << "evaluate subimages" << "\n";
    auto name = computed_input_frame.get().string();

    auto in = OIIO::ImageInput::open(name);
    int nsubimages = 0;
    while (in->seek_subimage(nsubimages, 0)) {
        result.push_back( in->spec().get_string_attribute("name"));
        ++nsubimages;
    }
    return result;
}, "subimages");

auto channels = Computed<std::vector<std::string>>([]() {
    return std::vector<std::string>();
}, "channels");

auto state_views = Computed<std::vector<std::string>>([]() {
    auto image_cache = OIIO::ImageCache::create(true);
    OIIO::ImageSpec spec;
    image_cache->get_imagespec(OIIO::ustring(computed_input_frame.get().string()), spec, selected_subimage.get(), 0);
    std::vector<std::string> views = get_stringvector_attribute(spec, "multiView");
    if (views.size() == 0) {
        return std::vector<std::string>();
    }
    return views;
}, "views");

auto groups = Computed<std::map<std::string, std::tuple<int, int>>>([]()->std::map<std::string, std::tuple<int, int>> {
    if (!std::filesystem::exists(computed_input_frame.get())) return {};

    auto name = computed_input_frame.get().string();
    
    auto in = OIIO::ImageInput::open(name);
    if (in->seek_subimage(selected_subimage.get(), 0))
    {
        auto image_cache = OIIO::ImageCache::create(true);
        OIIO::ImageSpec spec;
        image_cache->get_imagespec(OIIO::ustring(computed_input_frame.get().string()), spec, selected_subimage.get(), 0);

        std::map<std::string, std::tuple<int, int>> channel_names;
        for (auto c = 0; c < in->spec().nchannels; c++) {
            auto channel_name = in->spec().channel_name(c);
            std::string group_name = "other";
            if (channel_name == "R" || channel_name == "G" || channel_name == "B" || channel_name == "A") {
                group_name = "color";
            }
            else if (channel_name == "Z") {
                group_name = "depth";
            }
            else {
                auto channel_segments = split_string(channel_name, ".");
                if (channel_segments.size() > 1) {
                    std::cout << channel_name << ": ";
                    for (auto seg : channel_segments) std::cout << seg << ",";
                    group_name = join_string(channel_segments, ".", 0, channel_segments.size() - 1);
                    std::cout << " ->" << group_name;
                    std::cout << "\n";
                }
                else {
                    group_name = "other";
                }
            }
            
            if (!channel_names.contains(group_name)) {
                channel_names[group_name] = { c, c+1 };
            }
            else {
                auto& chrange = channel_names[group_name];

                if (c < std::get<0>(chrange)) {
                    std::get<0>(chrange) = c;
                }
                if (c > std::get<1>(chrange)) {
                    std::get<1>(chrange) = c+1;
                }
            }

        }
        return channel_names;
    }

    return {};
}, "groups");

auto texture = Computed<GLuint>([]()->GLuint {
    auto name = computed_input_frame.get().string();
    assert(("file does not exist", std::filesystem::exists(computed_input_frame.get())));

    auto in = OIIO::ImageInput::open(name);
    if (in->seek_subimage(selected_subimage.get(), 0))
    {
        std::cout << "get pixels" << "\n";
        auto image_cache = OIIO::ImageCache::create(true);
        OIIO::ImageSpec spec;
        image_cache->get_imagespec(OIIO::ustring(name), spec, selected_subimage.get(), 0);

        std::map<std::string, std::tuple<int, int>> group_map = groups.get();
        auto [chbegin, chend] = group_map[selected_group.get()];
        int nchannels = chend - chbegin;
        assert(("cannot display more than 4 channels", nchannels <= 4));

        std::cout << selected_group.get() << ": " << chbegin << "-" << chend << " #" << nchannels << "\n";
        auto x = spec.x;
        auto y = spec.y;
        int w = spec.width;
        int h = spec.height;
        float* data = (float*)malloc(spec.width * spec.height * nchannels * sizeof(float));
        image_cache->get_pixels(OIIO::ustring(name), selected_subimage.get(), 0, x, x+w, y, y+h, 0, 1, chbegin, chend,
            OIIO::TypeFloat,
            data,
            OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride,
            chbegin, chend);

        int format = GL_RGB;
        switch (nchannels) {
            case 1:
                format = GL_RED;
                break;
            case 2:
                format = GL_RG;
                break;
            case 3:
                format = GL_RGB;
                break;
            case 4:
                format = GL_RGBA;
        }
        std::cout << "make texture" << "\n";
        GLuint tex = imdraw::make_texture_float(
            spec.width, spec.height,
            data, // data
            format, // internal format
            format, // format
            GL_FLOAT, // type
            GL_LINEAR, //min_filter
            GL_NEAREST, //mag_filter
            GL_REPEAT, //wrap_s
            GL_REPEAT //wrap_t
        );

        if (nchannels == 1) {
            std::cout << "swizzle R" << "\n";
            glBindTexture(GL_TEXTURE_2D, tex);
            GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if(nchannels==2){
            std::cout << "swizzle RG" << "\n";
            glBindTexture(GL_TEXTURE_2D, tex);
            GLint swizzleMask[] = { GL_RED, GL_GREEN, GL_ZERO, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        free(data);

        return tex;
    }
    return 0;
}, "texture");

int main()
{
    glazy::init();
    while (glazy::is_running())
    {
        glazy::new_frame();
        // window visibility flags
        static bool show_channelstable_window{true};
        static bool show_viewer_window{ true };
        static bool show_info_window{ true };
        static bool show_dag_window{ true };
        static bool show_imagesequence_window{ true };
        static bool image_viewer_visible{ true };
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("windows"))
            {
                ImGui::MenuItem("channels sheet", "", &show_channelstable_window);
                ImGui::MenuItem("viewer", "", &show_viewer_window);
                ImGui::MenuItem("info", "", &show_info_window);
                ImGui::MenuItem("DAG", "", &show_dag_window);
                ImGui::MenuItem("image sequence", "", &show_imagesequence_window);
                ImGui::MenuItem("image viewer", "", &image_viewer_visible);

                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (image_viewer_visible && ImGui::Begin("Simple Viewer")) {
            ShowExrViewer();
        }

        if (show_dag_window && ImGui::Begin("DAG")) {
            layout_nodes();
            auto drawlist = ImGui::GetWindowDrawList();
            auto windowpos = ImGui::GetWindowPos();

            int i = 0;
            for (auto node : nodes) {
                drawlist->AddCircleFilled({ windowpos.x + node->pos.x, windowpos.y + node->pos.y }, 10, ImColor(255, 255, 255));
                drawlist->AddText({ windowpos.x + node->pos.x + 10, windowpos.y + node->pos.y - 20 }, ImColor(255, 255, 255), node->name.c_str());
                i++;
            }

            // draw edges
            for (auto node : nodes) {
                glm::vec2 P1 = { windowpos.x + node->pos.x, windowpos.y + node->pos.y };
                for (auto dep : node->dependants) {
                    glm::vec2 P2 = { windowpos.x + dep->pos.x, windowpos.y + dep->pos.y };

                    const float offset = 13.0f;
                    const float head_size = 5.0f;
                    auto forward = glm::normalize(P2 - P1);
                    auto right = glm::vec2(forward.y, -forward.x);
                    auto head = P2 - forward * offset;
                    auto tail = P1 + forward * offset;
                    auto right_wing = head - (forward * head_size) + (right * head_size);
                    auto left_wing = head - (forward * head_size) - (right * head_size);

                    drawlist->AddLine(ImVec2(tail.x, tail.y), ImVec2(head.x, head.y), ImColor(200, 200, 200), 1.0);
                    drawlist->AddLine(ImVec2(head.x, head.y), ImVec2(left_wing.x, left_wing.y), ImColor(200, 200, 200), 1.0);
                    drawlist->AddLine(ImVec2(head.x, head.y), ImVec2(right_wing.x, right_wing.y), ImColor(200, 200, 200), 1.0);
                }
            }

            ImGui::End();
        }

        if (show_info_window && ImGui::Begin("Read"))
        {
            if (ImGui::Button("open")) {
                auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0");
                if (!filepath.empty()) {
                    auto [pattern, first, last] = scan_for_sequence(filepath);
                    state_pattern.set(pattern);
                    selected_subimage = 0;

                    START_FRAME = first;
                    END_FRAME = last;
                    if (state_frame.get() < START_FRAME) state_frame.set(START_FRAME);
                    if (state_frame.get() > END_FRAME) state_frame.set(END_FRAME);
                }
            }

            static int slider_val = state_frame.get();
            if (ImGui::SliderInt("frame", &slider_val, START_FRAME, END_FRAME)) {
                state_frame.set(slider_val);
            }
            ImGui::Text("state_frame: %d", state_frame.get());
            ImGui::Text("state_pattern:  %s", state_pattern.get().string().c_str());
            ImGui::Text("frame filename: %s", computed_input_frame.get().string().c_str());

            if (!subimages.get().empty()) {
                if (ImGui::BeginCombo("subimage", subimages.get()[selected_subimage.get()].c_str())) {
                    for (auto s = 0; s < subimages.get().size(); s++) {
                        bool is_selected = s == selected_subimage.get();
                        if (ImGui::Selectable(subimages.get()[s].c_str(), is_selected)) {
                            selected_subimage.set(s);
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            if (!groups.get().empty()) {
                if (ImGui::BeginCombo("group", selected_group.get().c_str())) {

                    for (auto& [group, chrange] : groups.get()) {
                        bool is_selected = group == selected_group.get();

                        if (ImGui::Selectable(group.c_str(), is_selected)) {
                            selected_group.set(group);
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
            }

            // create a dataframe from all available channels
            // subimage <name,channelname> -> <layer,view,channel>

            if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None)) {
                if (ImGui::BeginTabItem("spec")) {
                    auto imagecache = OIIO::ImageCache::create(true);
                    OIIO::ImageSpec spec;
                    imagecache->get_imagespec(OIIO::ustring(computed_input_frame.get().string()), spec, selected_subimage.get(), 0);
                    auto info = spec.serialize(OIIO::ImageSpec::SerialText);
                    ImGui::Text("%s", info.c_str());
                    ImGui::EndTabItem();
                }
            }

            ImGui::End();
        }

        if (show_viewer_window && ImGui::Begin("Viewer")) {
            auto tex = texture.get();
            ImGui::Text("tex: %d", texture.get());
            ImGui::Image((ImTextureID)tex, {100,100});
            ImGui::End();
        }

        if (show_channelstable_window && ImGui::Begin("TestEXRLayers")) {
            TestEXRLayers();
            ImGui::End();
        }

        if (show_imagesequence_window && ImGui::Begin("Test ImageSequence")) {
            TestImageSequence();
            ImGui::End();
        }

        glazy::end_frame();

        for (auto tex : textures_to_delete) {
            glDeleteTextures(1, &tex);
        }
        textures_to_delete.clear();
    }
    glazy::destroy();
    std::cout << "Hello World!\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
