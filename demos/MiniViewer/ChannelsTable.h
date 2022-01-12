#pragma once

// data structures
#include <tuple>
#include <string>
#include <vector>

// std libraries
#include <cassert>
#include <filesystem>
#include <iostream>

// from glazy
#include "stringutils.h"

// OpenImageIO
#include "OpenImageIO/imageio.h""
#include "OpenImageIO/imagecache.h"

int indexOf(const std::vector<std::string>& v, const std::string element) {
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

/// define Channel Types
using ChannelKey = std::tuple<int, int>;                                 // subimage, channel index
using ChannelRecord = std::tuple<std::string, std::string, std::string>; // layer.view.channel
using ChannelsTable = std::map<ChannelKey, ChannelRecord>;

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
ChannelsTable get_channelstable(const std::filesystem::path& filename)
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

/// Get index column
std::vector<ChannelKey> get_index_column(const ChannelsTable& df) {
    std::vector<ChannelKey> indices;
    for (auto [key, value] : df) {
        indices.push_back(key);
    }
    return indices;
}

/// Get index-subimage Column
std::vector<int> get_subimage_idx_column(const ChannelsTable& df) {
    std::vector<int> subimages;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;

        subimages.push_back(subimage);
    }
    return subimages;
}

/// Get index-chan Column
std::vector<int> get_chan_idx_column(const ChannelsTable& df) {
    std::vector<int> chans;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;

        chans.push_back(chan);
    }
    return chans;
}

/// Get layers Column
std::vector<std::string> get_layers_column(const ChannelsTable& df) {
    std::vector<std::string> layers;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;

        layers.push_back(layer);
    }
    return layers;
}

/// Get views Column
std::vector<std::string> get_views_column(const ChannelsTable& df) {
    std::vector<std::string> views;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;

        views.push_back(view);
    }
    return views;
}

/// Get channels Column
std::vector<std::string> get_channels_column(const ChannelsTable& df) {
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
std::map<std::tuple<std::string, std::string>, ChannelsTable> group_by_layer_view(const ChannelsTable& df) {
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
std::map<std::string, ChannelsTable> group_by_layer(const ChannelsTable& df) {
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
std::map<std::string, ChannelsTable> group_by_view(const ChannelsTable& df) {
    std::map<std::string, ChannelsTable> view_groups;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [layer, view, channel] = layer_view_channel;


        if (!view_groups.contains(view)) view_groups[view] = ChannelsTable();

        view_groups[view][subimage_chan] = layer_view_channel;
    }
    return view_groups;
}
