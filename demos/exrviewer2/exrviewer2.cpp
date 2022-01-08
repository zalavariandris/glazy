// exrviewer2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <set>
#include <deque>
#include <any>
#include <cassert>
#include <ranges>

// glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"

// OpenImageIO
#include "OpenImageIO/imageio.h""
#include "OpenImageIO/imagecache.h"

/* EXR helpers */
using AttributeVariant = std::variant<                                   // either a string or a vector of strings | int, vector of int... | float, vector of float | 
    std::string, std::vector<std::string>, 
    int, std::vector<int>, 
    float, std::vector<float>>;
using ChannelKey = std::tuple<int, int>;                                 // subimage, channel index
using ChannelRecord = std::tuple<std::string, std::string, std::string>; // layer.view.channel
using ChannelsTable = std::map<ChannelKey, ChannelRecord>;

// subimage, channel index
using View = std::string;                           // view name or empty string
using Layer = std::map<View, std::vector<ChannelKey>>; // vector of channels by views
using FrameRange = std::tuple<int, int>;            // firstFrame-lastFrame

struct Image {
    std::string name;
    FrameRange framerange;
    std::map<std::string, Layer> layers;
};

/// Retrieve any metadata attribute, converted to a stringvector./
/// If no such metadata exists, the `defaultval` will be returned.
std::vector<std::string> get_stringvector_attribute(const OIIO::ImageSpec& spec, std::string name, std::vector<std::string> default_value = std::vector<std::string>()) {
    // TODO: handle errors: cant find attribute. attribute is not a stringvector
    std::vector<std::string> result;
    auto attribute_type = spec.getattributetype(name);
    if (attribute_type.is_sized_array()) {
        const char** data = (const char**)malloc(attribute_type.basesize() * attribute_type.basevalues());
        if (data) {
            spec.getattribute(name, attribute_type, data);
            for (auto i = 0; i < attribute_type.arraylen; i++) {
                result.push_back(data[i]);
            }
            free(data);
        }
    }
    return result;
}



/// parse exr channel names with format: layer.view.channel
/// return a tuple in the above order.
/// 
/// tests:
/// - R; right,left -> color right R
/// - Z; right,left  -> depth right Z
/// - left.R; right,left  -> color left R
/// - left.Z; right,left  -> depth left Z
/// - disparityR.x -> disparityL _ R
ChannelRecord parse_channel_name(std::string channel_name, std::vector<std::string> views_hint) {
    bool isMultiView = !views_hint.empty();

    auto channel_segments = split_string(channel_name, ".");

    std::tuple<std::string, std::string, std::string> result;

    if (channel_segments.size() == 1) {
        std::string channel = channel_segments.back();
        bool isColor = std::string("RGBA").find(channel) != std::string::npos;
        bool isDepth = channel == "Z";
        std::string layer = "[other]";
        if (isColor) layer = "[color]";
        if (isDepth) layer = "[depth]";
        std::string view = isMultiView ? views_hint[0] : "";
        return std::tuple<std::string, std::string, std::string>({ layer,view,channel });
    }

    if (channel_segments.size() == 2) {
        // find a view name right before the final channel name. If not found this channel is not associated with any view
        bool IsInView = std::find(views_hint.begin(), views_hint.end(), channel_segments.end()[-2]) != views_hint.end();

        if (IsInView) {
            std::string channel = channel_segments.back();
            bool isColor = std::string("RGBA").find(channel) != std::string::npos;
            bool isDepth = channel == "Z";
            std::string layer = "[other]";
            if (isColor) layer = "[color]";
            if (isDepth) layer = "[depth]";
            return { layer, channel_segments.end()[-2], channel_segments.back() };
        }
        else {
            return { channel_segments[0], "[data]", channel_segments.back() };
        }
    }

    if (channel_segments.size() == 3) {
        // find a view name right before the final channel name. If not found this channel is not associated with any view
        bool IsInView = std::find(views_hint.begin(), views_hint.end(), channel_segments.end()[-2]) != views_hint.end();

        if (IsInView) {
            // this channel is in a view
            // this is the format descriped in oenexr docs: https://www.openexr.com/documentation/MultiViewOpenEXR.pdf
            //{layer}.{view}.{final channels}
            return { channel_segments[0], channel_segments.end()[-2], channel_segments.back() };
        }
        else {
            // this channel is not in a view, but the layer name contains a dot
            //{layer.name}.{final channels}
            auto layer = join_string(channel_segments, ".", 0, channel_segments.size() - 2);
            return { layer, "[data]", channel_segments.back() };
        }
    }

    if (channel_segments.size() > 3) {
        // find a view name right before the final channel name. If not found this channel is not associated with any view
        bool IsInView = std::find(views_hint.begin(), views_hint.end(), channel_segments.end()[-2]) != views_hint.end();

        if (IsInView) {
            auto layer = join_string(channel_segments, ".", 0, channel_segments.size() - 3);
            auto view = channel_segments.end()[-2];
            auto channel = channel_segments.end()[-1];
            return { layer, view, channel };
        }
        else {
            auto layer = join_string(channel_segments, ".", 0, channel_segments.size() - 2);
            auto view = "[data]";
            auto channel = channel_segments.end()[-1];
            return { layer, view, channel };
        }
    }
}

/// Read ChannelsTable from an exr file
///  test file formats: exr1, exr2, dpx, jpg
///  test single part singleview
///  test single part multiview
///  test multipart singleview
///  test multipart multiview
ChannelsTable get_channels_dataframe(std::filesystem::path filename)
{

    auto image_cache = OIIO::ImageCache::create(true);

    // create a dataframe from all available channels
    // subimage <name,channelname> -> <layer,view,channel>
    std::map<std::tuple<int, int>, std::tuple<std::string, std::string, std::string>> layers_dataframe;
    
    auto in = OIIO::ImageInput::open(filename.string());
    auto spec = in->spec();
    std::vector<std::string> views = get_stringvector_attribute(spec, "multiView");
    int nsubimages = 0;

    while (in->seek_subimage(nsubimages, 0)) {
        auto spec = in->spec();
        std::string subimage_name = spec.get_string_attribute("name");
        for (auto chan = 0; chan < spec.nchannels; chan++) {

            std::string channel_name = spec.channel_name(chan);
            const auto& [layer, view, channel] = parse_channel_name(spec.channel_name(chan), views);
            std::tuple<int, int> key{ nsubimages, chan };
            layers_dataframe[key] = { layer, view, channel };
        }
        ++nsubimages;
    }

    return layers_dataframe;
}

/// Get index-subimage Column
std::vector<int> get_subimage_idx_column(ChannelsTable df) {
    std::vector<int> subimages;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;

        subimages.push_back(subimage);
    }
    return subimages;
}

/// Get index-chan Column
std::vector<int> get_chan_idx_column(ChannelsTable df) {
    std::vector<int> chans;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;

        chans.push_back(chan);
    }
    return chans;
}

/// Get layers Column
std::vector<std::string> get_layers_column(ChannelsTable df) {
    std::vector<std::string> layers;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;

        layers.push_back(layer);
    }
    return layers;
}

/// Get views Column
std::vector<std::string> get_views_column(ChannelsTable df) {
    std::vector<std::string> views;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;

        views.push_back(layer);
    }
    return views;
}

/// Get channels Column
std::vector<std::string> get_channels_column(ChannelsTable df) {
    std::vector<std::string> channels;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;

        channels.push_back(channel);
    }
    return channels;
}

/* Group by */
/// Group channels by layer and view
std::map<std::tuple<std::string, std::string>, ChannelsTable> group_by_layer_view(const ChannelsTable & df) {
    std::map<std::tuple<std::string, std::string>, ChannelsTable> layer_view_groups;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;

        std::tuple<std::string, std::string> key{ layer, view };
        if (!layer_view_groups.contains(key)) layer_view_groups[key] = ChannelsTable();

        layer_view_groups[key][subimage_chan] = layer_view_channel;
    }
    return layer_view_groups;
}

/// Group channels by layer
std::map<std::string, ChannelsTable> group_by_layer(ChannelsTable df) {
    std::map<std::string, ChannelsTable> layer_groups;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;


        if (!layer_groups.contains(layer)) layer_groups[layer] = ChannelsTable();

        layer_groups[layer][subimage_chan] = layer_view_channel;
    }
    return layer_groups;
}

/// Group channels by view
std::map<std::string, ChannelsTable> group_by_view(ChannelsTable df) {
    std::map<std::string, ChannelsTable> view_groups;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;


        if (!view_groups.contains(view)) view_groups[view] = ChannelsTable();

        view_groups[view][subimage_chan] = layer_view_channel;
    }
    return view_groups;
}

/// Gui for ChannelsTable
bool ImGui_ChannelsTable(const ChannelsTable& channels_dataframe) {
    if (ImGui::BeginTable("channels table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("subimage/chan");
        ImGui::TableSetupColumn("layer");
        ImGui::TableSetupColumn("view");
        ImGui::TableSetupColumn("channel");
        ImGui::TableHeadersRow();
        for (const auto& [subimage_chan, layer_view_channel] : channels_dataframe) {
            auto [subimage, chan] = subimage_chan;
            auto [layer, view, channel] = layer_view_channel;

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
        ImGui::EndTable();
    }
}

void ImGui_Display(std::vector<int> values) {
    ImGui::BeginGroup();
    for (auto val : values) {
        ImGui::Text("%d", val);
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::EndGroup();
}


int indexOf(const std::vector<std::string> &v, const std::string element) {
    auto it = find(v.begin(), v.end(), element);

    // If element was found
    if (it != v.end())
    {
        int index = it - v.begin();
        return index;
    }
    else {
        return -1;
    }
}

bool ImGui_sanitycheck_channelstable(const ChannelsTable &df) {

}

void ImGui_DisplayImage(const ChannelsTable &df) {

}

void TestEXRLayers() {
    std::filesystem::path filename = "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr";



    // Read channels dataframe
    ChannelsTable channels_table = get_channels_dataframe(filename);
    static ChannelsTable selected_df;

    // Group by layers and views
    std::map<std::tuple<std::string, std::string>, ChannelsTable> layer_view_groups = group_by_layer_view(channels_table);
    std::map<std::string, ChannelsTable> layer_groups = group_by_layer(channels_table); // split channels to layers
    std::map<std::string, ChannelsTable> view_groups = group_by_view(channels_table);   // split channels to layers
    std::map<std::string, std::map<std::string, ChannelsTable>> layer_view_map; // split channels to layers with views
    for (const auto& [layer, channels] : group_by_layer(channels_table)) {
        layer_view_map[layer] = group_by_view(channels);
    }

    // Show filename
    ImGui::Text("filename: %s", filename.string().c_str());
    ImGui::Separator();

    // Show Views
    auto image_cache = OIIO::ImageCache::create(true);
    OIIO::ImageSpec spec;
    image_cache->get_imagespec(OIIO::ustring(filename.string()), spec, 0, 0);
    auto views = get_stringvector_attribute(spec, "multiView");

    // display views
    ImGui::Text("views: "); ImGui::SameLine();
    for (auto view : views) {
        ImGui::Text(view.c_str()); ImGui::SameLine();
    } ImGui::NewLine();

    ImGui::Separator();

    // Show DataFrame
    if (ImGui::BeginChild("channels_column", {ImGui::GetContentRegionAvailWidth()-256, ImGui::GetContentRegionAvail().y})) {
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

            if (ImGui::BeginTabItem("layer -> view")) {
                for (auto [layer, view_groups] : layer_view_map) {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (ImGui::TreeNode(layer.c_str())) {
                        for (auto [view, df] : view_groups) {
                            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                            if (ImGui::TreeNode(view.c_str())) {
                                // check if channels are the same subimage
                                std::vector<int> subimages = get_subimage_idx_column(selected_df);
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

                                if (ImGui::Selectable(layer.c_str())) {
                                    selected_df = df;
                                }
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
    if(ImGui::BeginChild("viewer column", {256, ImGui::GetContentRegionAvail().y})) {
        // show selected channels
        ImGui::Text("%s", "display channels");

        std::vector<int> subimages = get_subimage_idx_column(selected_df);
        std::vector<int> channels = get_chan_idx_column(selected_df);

        ImGui::Text("subimages:");
        bool AreTheSameSubmages = std::all_of(subimages.begin(), subimages.end(), [&](int i) {return i == subimages[0]; });
        ImGui_Display(subimages); ImGui::TextColored(AreTheSameSubmages ? ImColor(0, 255, 0) : ImColor(255, 0, 0), AreTheSameSubmages ? "SameSubimage" : "Channels shoud come from the same subimage");
        ImGui::Text("channels:");

        bool IsConsecutive = true;
        for (int i = 1; i < channels.size(); ++i) {
            if (channels[i] != channels[i - 1] + 1) {
                IsConsecutive = false;
                break;
            }
        }

        bool WithinMaximumChannels = channels.size() <= 4;
        ImGui_Display(channels);
        ImGui::TextColored(IsConsecutive ? ImColor(0, 255, 0) : ImColor(255, 0, 0), IsConsecutive ? "Channels are in order" : "Channels shoud be in order!");
        ImGui::TextColored(WithinMaximumChannels ? ImColor(0, 255, 0) : ImColor(255, 0, 0), WithinMaximumChannels ? "Channels can be displayed in RGBA" : "Maximum four channels can be displayed: RGBA");


        if (subimages.size() > 0 && AreTheSameSubmages && channels.size() > 0 && IsConsecutive && WithinMaximumChannels) {
            ImGui::Text("!!!DISPLAY IMAGE!!!");
            // read header
            auto image_cache = OIIO::ImageCache::create(true);
            OIIO::ImageSpec spec;
            image_cache->get_imagespec(OIIO::ustring(filename.string()), spec, subimages[0], 0);
            auto x = spec.x;
            auto y = spec.y;
            int w = spec.width;
            int h = spec.height;
            int chbegin = channels[0];
            int chend = channels[channels.size() - 1] + 1; // channel range is exclusive [0-3)
            int nchannels = chend - chbegin;
            ImGui::Text("pos: %d %d, size: %dx%d, channels: %d-%d #%d", x, y, w, h, chbegin, chend, nchannels);

            // read pixels
            float* data = (float*)malloc(w * h * nchannels * sizeof(float));
            image_cache->get_pixels(OIIO::ustring(filename.string()), subimages[0], 0, x, x + w, y, y + h, 0, 1, chbegin, chend, OIIO::TypeFloat, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, chbegin, chend);

            // create texture
            static GLuint tex;
            if (glIsTexture(tex)) {
                glDeleteTextures(1, &tex);
            }

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
            glTexImage2D(GL_TEXTURE_2D, 0, internalformat, w, h, 0, format, type, data);
            glBindTexture(GL_TEXTURE_2D, 0);

            // free data from memory after uploaded to gpu
            free(data);
            ImGui::Image((ImTextureID)tex, ImVec2(256, 256));
        }
        ImGui::EndChild();
    }
}



/**********************
 * Reactive lazy vars *
 **********************/
class BaseVar {
protected:
    mutable bool dirty=true;
public:
    mutable std::set<BaseVar*> dependants;
    std::string name;
    ImVec2 pos={0,0};
    void set_dirty(bool val=true) {
        dirty = val;
    }
};

std::vector<BaseVar*>depstack;
std::set<BaseVar*> nodes;

template <typename T>
class State : public BaseVar {
private:
    mutable T cache;
public:
    State(T val, std::string _name="") :cache(val) {
        nodes.insert(this);
        name = _name;
    };
    void set(T val) {
        cache = val;
        for (auto dep : dependants) dep->set_dirty();
    }
    T get() const {
        if (!depstack.empty()) {
            dependants.insert(depstack.back());
        }
        return cache;
    }
};

template <typename T>
class Computed : public BaseVar{
private:
    mutable T cache;
    std::function<T()> evaluate;
public:
    Computed(std::function<T()> f, std::string _name="") : evaluate(f) {
        nodes.insert(this);
        name = _name;
    };

    const T get() const {
        if (!depstack.empty()) {
            dependants.insert(depstack.back());
        }
        if (dirty) {
            depstack.push_back((BaseVar*)this);
            cache = evaluate();
            depstack.pop_back();
            dirty = false;
            for (auto dep : dependants) dep->set_dirty();
        }
        
        return cache;
    }
};

/*********
 * Store * 
 *********/

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

// widget helpers
void layout_nodes() {
    // collect edges
    std::set<std::tuple<BaseVar*, BaseVar*>> edges;
    for (auto node : nodes) {
        for (auto dep : node->dependants) {
            edges.insert({ node, dep });
        }
    }

    std::map<int, std::set<BaseVar*>> layers;
    

    // Find Roots
    std::vector<BaseVar*> roots;
    for (auto node : nodes) roots.push_back(node);
    // collect nodes with depencies
    std::set<BaseVar*> nodes_with_dependencies;
    for (auto node : nodes) {
        for (auto dep : node->dependants) {
            nodes_with_dependencies.insert(dep);
        }
    }
    // remove nodes with dependencies from queue->keep root nodes only!
    roots.erase(std::remove_if(roots.begin(), roots.end(), [&](auto n) {
        return nodes_with_dependencies.contains(n);
    }), roots.end());

    auto Q = roots;
    std::set<BaseVar*> visited;
    int current_layer = 0;
    while (!Q.empty()) {
        auto v = Q.back(); Q.pop_back();
        layers[current_layer].insert(v);

        for (auto w : v->dependants) {
            if (!visited.contains(w)) {
                current_layer += 1;
                visited.insert(w);
                Q.push_back(w);
            }
        }
    }

    // position nodes on layers
    for (auto [u, layer]:layers) {
        int v = 0;
        for (auto node : layer) {
            node->pos = { v*150.f+50, u*120.f+50 };
            v += 1;
        }
    }

    //for (auto node : nodes) {
    //    node->pos.x = (float)rand() / RAND_MAX * 300 + 50;
    //    node->pos.y = (float)rand() / RAND_MAX * 300 + 50;
    //}
}

int main()
{
    glazy::init();
    while (glazy::is_running())
    {
        glazy::new_frame();

        if (ImGui::Begin("DAG")) {
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

        if (ImGui::Begin("Read"))
        {
            if (ImGui::Button("open")) {
                auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0");
                if (!filepath.empty()) {
                    auto [pattern, first, last] = scan_sequence(filepath);
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

            if (ImGui::Begin("tests")) {
                if (ImGui::CollapsingHeader("TestEXRLayers")) {
                    TestEXRLayers();
                }
                ImGui::End();
            }

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

        if (ImGui::Begin("Viewer")) {
            auto tex = texture.get();
            ImGui::Text("tex: %d", texture.get());
            ImGui::Image((ImTextureID)tex, {100,100});
            ImGui::End();
        }
        glazy::end_frame();
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
