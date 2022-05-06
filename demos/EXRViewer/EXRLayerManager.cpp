#include "EXRLayerManager.h"

#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfStdIO.h>
#include <OpenEXR/ImfChannelList.h>

#include <vector>
#include <array>

#include <filesystem>
#include <assert.h>
#include <vector>
#include <iostream>
#include <string>
#include <format>
#include <optional>

#include "stringutils.h"
#include <sstream>

#include <algorithm>

std::vector<std::tuple<int, int>> group_stringvector_by_patterns(const std::vector<std::string>& statement, const std::vector<std::vector<std::string>>& patterns)
{
    std::vector<std::tuple<int, int>> groups;

    // sort patterns
    auto sorted_patterns = patterns;
    std::sort(sorted_patterns.begin(), sorted_patterns.end(), [](const std::vector<std::string>& A, const std::vector<std::string>& B) {
        return A.size() > B.size();
        });

    // collect pattern matches
    int i = 0;
    while (i < statement.size())
    {
        std::optional<std::tuple<int, int>> match;

        // check all patterns at current index
        // if match is found, move iterator to the end of match
        for (std::vector<std::string> pattern : sorted_patterns) {
            auto begin = i;
            auto end = i + pattern.size();
            auto statement_sample = std::vector<std::string>(statement.begin() + begin, statement.begin() + std::min(end, statement.size()));
            bool IsMatch = pattern == statement_sample;
            if (IsMatch)
            {
                match = std::tuple<int, int>(begin, end);
                i = end - 1;
                break;
            }
        }

        // if match is found, add it to the stack
        // otherwise, keep single item 
        if (match)
        {
            groups.push_back(match.value());
        }
        else {
            groups.push_back(std::tuple<int, int>(i, i + 1));
        }
        i++;
    }
    return groups;
}

std::vector<Layer> Layer::split_by_patterns(const std::vector<std::vector<std::string>>& patterns) const {
    std::vector<Layer> sublayers;

    for (auto span : group_stringvector_by_patterns(mChannels, patterns)) {
        auto [begin, end] = span;
        auto channels = std::vector<std::string>(mChannels.begin() + begin, mChannels.begin() + end);
        sublayers.push_back(sublayer(channels));
    }

    return sublayers;
}

std::tuple<std::string, std::string> Layer::split_channel_id(const std::string& channel_id)
{
    size_t found = channel_id.find_last_of(".");
    auto viewlayer = found != std::string::npos ? channel_id.substr(0, found) : "";
    auto channel = channel_id.substr(found + 1);
    return { viewlayer, channel };
}


std::string Layer::part_name() const {
    return mPart_name;
}

int Layer::part_idx() const {
    return mPart_idx;
}

/// the layer name
std::string Layer::name() const {
    return mName;
}

std::string Layer::path() const {
    return part_name() + "." + name();
}

/// full EXR channel name
std::vector<std::string> Layer::channel_ids() const {
    std::vector<std::string> full_names;
    for (auto channel : mChannels) {
        full_names.push_back(mName.empty() ? channel : join_string({ mName, channel }, "."));
    }
    return full_names;
}

std::vector<std::string> Layer::channels() const {
    return mChannels;
}

Layer Layer::sublayer(std::vector<std::string> subchannels, std::string sublayer_name, bool compose) const
{
    // filter requested subchannels to this layer channels.
    std::erase_if(subchannels, [&](std::string c) {
        return std::find(this->channels().begin(), this->channels().end(), c) == this->channels().end();
    });

    //// compose part and layer name
    if (compose) {
        std::vector<std::string> names{ this->mName, sublayer_name };
        names.erase(std::remove_if(names.begin(), names.end(), [](const std::string& s) {return s.empty(); }), names.end());
        sublayer_name = join_string(names, ".");
    }

    return Layer(sublayer_name, subchannels, this->part_name(), this->part_idx());
}

std::vector<Layer> Layer::split_by_delimiter() const {
    std::vector<Layer> sublayers;

    for (std::string channel_id : mChannels) {
        auto [viewlayer, channel] = Layer::split_channel_id(channel_id);



        if (sublayers.empty())
        {
            sublayers.push_back(sublayer({}, viewlayer));
        }

        if (sublayers.back().mName == viewlayer) {

        }
        else
        {
            sublayers.push_back(sublayer({}, viewlayer));
        }

        sublayers.back().mChannels.push_back(channel);
    }

    return sublayers;
}



/// nice name
std::string Layer::display_name() const {
    std::stringstream ss;

    auto part_idx = this->part_idx();
    auto part_name = this->part_name();
    auto layer_name = this->name();
    auto channels = this->channels();

    std::string name;

    if (part_name != layer_name) {
        if (!part_name.empty() && !layer_name.empty()) {
            ss << part_name << "/" << layer_name;
        }
        else {
            ss << part_name << layer_name;
        }
    }
    else {
        ss << layer_name;
    }

    ss << " (";
    for (auto c : this->mChannels) ss << c;
    ss << ")";
    return ss.str();
}

std::vector<Layer> get_all_part_layers(const Imf::MultiPartInputFile& file)
{
    std::vector<Layer> layers;

    /// parts as layers
    {
        std::vector<Layer> partlayers;
        std::vector<std::string> part_names;
        for (auto p = 0; p < file.parts(); p++)
        {
            auto header = file.header(p);

            std::string part_name = file.header(p).hasName() ? file.header(p).name() : "";
            auto cl = header.channels();
            std::vector<std::string> channel_names;
            for (Imf::ChannelList::ConstIterator c = cl.begin(); c != cl.end(); ++c) channel_names.push_back(c.name());

            auto lyr = Layer("", channel_names, part_name, p);
            partlayers.push_back(lyr);
        }
        layers = partlayers;
    }
    //return layers;

    /// split by channel delimiters
    {
        std::vector<Layer> sublayers;
        for (auto lyr : layers) {
            for (auto sublyr : lyr.split_by_delimiter()) {
                sublayers.push_back(sublyr);
            }
        }
        layers = sublayers;
    }
    //return layers;

    /// split by patters
    {
        const std::vector<std::vector<std::string>> patterns = { {"red", "green", "blue"},{"A", "B", "G", "R"},{"B", "G", "R"},{"x", "y"},{"u", "v"},{"u", "v", "w"} };
        std::vector<Layer> sublayers;
        for (auto lyr : layers) {
            for (auto sublayer : lyr.split_by_patterns(patterns)) {
                sublayers.push_back(sublayer);
            }
        }
        layers = sublayers;
    }

    return layers;
}

EXRLayerManager::EXRLayerManager(const std::filesystem::path& filename)
{
    auto file = Imf::MultiPartInputFile(filename.string().c_str());
    mLayers = get_all_part_layers(file);
}

std::vector<std::string> EXRLayerManager::names()
{
    std::vector<std::string> names;
    for (auto lyr : mLayers) {
        names.push_back(lyr.display_name());
    }
    return names;
}

void EXRLayerManager::set_current(int i)
{
    assert(("out of range", i < mLayers.size()));
    current_layer_idx = i;
}

int EXRLayerManager::current() {
    return current_layer_idx;
}

std::string EXRLayerManager::current_name() {
    return mLayers.at(current_layer_idx).display_name();
}

std::vector<std::string> EXRLayerManager::current_channel_ids() {
    return mLayers.at(current_layer_idx).channel_ids();
}

int EXRLayerManager::current_part_idx() {
    return mLayers.at(current_layer_idx).part_idx();
}


