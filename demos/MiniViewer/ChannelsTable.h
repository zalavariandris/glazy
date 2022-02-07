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
using ChannelRecord = std::tuple<std::string, std::string, std::string, std::string, std::string>; // subname, subview | layer.view.channel
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
            
        }
        free(data);
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
/// 

#define COLOR_LAYER ""
#define DEPTH_LAYER ""
#define OTHER_LAYER ""
#define DATA_VIEW ""

std::tuple<std::string, std::string, std::string> parse_channel_name(const std::string& channel_name, const std::vector<std::string>& views_hint) {
    bool isMultiView = !views_hint.empty();

    auto channel_segments = split_string(channel_name, ".");

    std::tuple<std::string, std::string, std::string> result;

    if (channel_segments.size() == 1) {
        std::string channel = channel_segments.back();
        bool isColor = std::string("RGBA").find(channel) != std::string::npos;
        bool isDepth = channel == "Z";
        std::string layer = OTHER_LAYER;
        if (isColor) layer = COLOR_LAYER;
        if (isDepth) layer = DEPTH_LAYER;
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
            std::string layer = OTHER_LAYER;
            if (isColor) layer = COLOR_LAYER;
            if (isDepth) layer = DEPTH_LAYER;
            return { layer, channel_segments.end()[-2], channel_segments.back() };
        }
        else {
            return { channel_segments[0], DATA_VIEW, channel_segments.back() };
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
            return { layer, DATA_VIEW, channel_segments.back() };
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
            auto view = DATA_VIEW;
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
    if (!std::filesystem::exists(filename)) {
        return {};
    }

    // create a dataframe from all available channels
    // subimage <name,channelname> -> <layer,view,channel>
    ChannelsTable layers_dataframe;


    auto image_cache = OIIO::ImageCache::create(true);
    int nsubimages;
    image_cache->get_image_info(OIIO::ustring(filename.string().c_str()), 0, 0, OIIO::ustring("subimages"), OIIO::TypeInt, &nsubimages);

    OIIO::ImageSpec spec;
    image_cache->get_imagespec(OIIO::ustring(filename.string().c_str()), spec, 0, 0);
    std::vector<std::string> views = get_stringvector_attribute(spec, "multiView");

    for (int i = 0; i < nsubimages; i++) {
        OIIO::ImageSpec spec;
        image_cache->get_imagespec(OIIO::ustring(filename.string().c_str()), spec, i, 0);
        std::string subimage_name = spec.get_string_attribute("name");
        std::string subimage_view = spec.get_string_attribute("view");
        if (ends_with(subimage_name, subimage_view))
        {
            subimage_name = subimage_name.substr(0, subimage_name.size() - subimage_view.size());
            subimage_name = trim(subimage_name, " _.-");
        }

        for (auto chan = 0; chan < spec.nchannels; chan++)
        {
            std::string channel_name = spec.channel_name(chan);
            auto [layer, view, channel] = parse_channel_name(spec.channel_name(chan), views);
            //std::cout << "  layer:" << layer << " sub-name: " << subimage_name << "\n";
            //std::cout << "  view:" << view << " sub-view: " << subimage_view << "\n";

            // figure out layer name
            if (i == 0 && layer.empty())
            {
                layer = subimage_name;
            }
            else if (subimage_name == layer) {

            }
            else if (layer.empty()) {
                layer = subimage_name;
            }
            else {
                if (subimage_name != "rgba") {
                    layer = subimage_name + "." + layer;
                }
            }

            // split depth channel only with RGBAZ colors. keep XYZ as one layer


            // figure out view name
            if (view.empty()) {
                view = subimage_view;
            }

            std::tuple<int, int> key{ i, chan };
            layers_dataframe.insert({ key, {subimage_name, subimage_view, layer, view, channel} });
        }
    }

    OIIO::ImageCache::destroy(image_cache);
    return layers_dataframe;
}

ChannelsTable get_rows(const ChannelsTable& df, const std::vector<ChannelKey>& keys) {
    ChannelsTable result;
    for (auto [idx, record] : df) {
        if (std::find(keys.begin(), keys.end(), idx) != keys.end())
        {
            result.insert({ idx, record });
        }
    }
    return result;
}

/// Get index column
std::vector<ChannelKey> get_index_column(const ChannelsTable& df)
{
    std::vector<ChannelKey> indices;
    for (auto [key, value] : df) {
        indices.push_back(key);
    }
    return indices;
}

/// Get index-subimage Column
std::vector<int> get_subimage_idx_column(const ChannelsTable& df)
{
    std::vector<int> subimages;
    for (auto&& [subimage_chan, layer_view_channel] : df)
    {
        auto&& [subimage, chan] = subimage_chan;

        subimages.push_back(subimage);
    }
    return subimages;
}

/// Get index-chan Column
std::vector<int> get_chan_idx_column(const ChannelsTable& df)
{
    std::vector<int> chans;
    for (auto&& [subimage_chan, layer_view_channel] : df)
    {
        auto&& [subimage, chan] = subimage_chan;

        chans.push_back(chan);
    }
    return chans;
}

/// Get layers Column
std::vector<std::string> get_layers_column(const ChannelsTable& df)
{
    std::vector<std::string> layers;
    for (auto&& [subimage_chan, layer_view_channel] : df)
    {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [subimage_name, subimage_view, layer, view, channel] = layer_view_channel;

        layers.push_back(layer);
    }
    return layers;
}

/// Get views Column
std::vector<std::string> get_views_column(const ChannelsTable& df)
{
    std::vector<std::string> views;
    for (auto&& [subimage_chan, layer_view_channel] : df)
    {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [subimage_name, subimage_view, layer, view, channel] = layer_view_channel;

        views.push_back(view);
    }
    return views;
}

/// Get channels Column
std::vector<std::string> get_channels_column(const ChannelsTable& df)
{
    std::vector<std::string> channels;
    for (auto&& [subimage_chan, layer_view_channel] : df)
    {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [subimage_name, subimage_view, layer, view, channel] = layer_view_channel;

        channels.push_back(channel);
    }
    return channels;
}

/* filter by */
ChannelsTable filtered_by_layer(const ChannelsTable& df, const std::string& filter_layer)
{
    ChannelsTable filtered_df;
    for (const auto& [idx, record] : df)
    {
        auto&& [subimage_name, subimage_view, layer, view, channel] = record;
        if (layer == filter_layer) {
            filtered_df.insert({ idx,record });
        }
    }
    return filtered_df;
}

ChannelsTable filtered_by_layer_and_view(const ChannelsTable& df, const std::string& filter_layer, const std::string& filter_view)
{
    ChannelsTable filtered_df;
    for (const auto& [idx, record] : df) {
        auto&& [subimage_name, subimage_view, layer, view, channel] = record;
        if (layer == filter_layer && view == filter_view) {
            filtered_df.insert({ idx,record });
        }
    }
    return filtered_df;
}

/* Group by */
/// Group channels by layer and view
std::map<std::tuple<std::string, std::string>, ChannelsTable> group_by_layer_view(const ChannelsTable& df)
{
    std::map<std::tuple<std::string, std::string>, ChannelsTable> layer_view_groups;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [subimage_name, subimage_view, layer, view, channel] = layer_view_channel;

        std::tuple<std::string, std::string> key{ layer, view };
        if (!layer_view_groups.contains(key)) layer_view_groups.insert({ key, ChannelsTable() });

        layer_view_groups.at(key).insert({ subimage_chan, layer_view_channel });
    }
    return layer_view_groups;
}

/// Group channels by layer
std::map<std::string, ChannelsTable> group_by_layer(const ChannelsTable& df)
{
    std::map<std::string, ChannelsTable> layer_groups;
    for (auto&& [subimage_chan, layer_view_channel] : df)
    {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [subimage_name, subimage_view, layer, view, channel] = layer_view_channel;


        if (!layer_groups.contains(layer)) layer_groups.insert({ layer, ChannelsTable() });

        layer_groups.at(layer).insert({ subimage_chan, layer_view_channel });
    }
    return layer_groups;
}

/// Group channels by view
std::map<std::string, ChannelsTable> group_by_view(const ChannelsTable& df)
{
    std::map<std::string, ChannelsTable> view_groups;
    for (auto&& [subimage_chan, layer_view_channel] : df) {
        auto&& [subimage, chan] = subimage_chan;
        auto&& [subimage_name, subimage_view, layer, view, channel] = layer_view_channel;


        if (!view_groups.contains(view)) view_groups.insert({ view, ChannelsTable() });

        view_groups.at(view).insert({ subimage_chan, layer_view_channel });
    }
    return view_groups;
}
