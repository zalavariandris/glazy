#pragma once

#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfStdIO.h>

#include <vector>
#include <array>
#include "Layer.h"

class LayerManager
{
    int current_layer_idx = 0;
    //bool group_by_patterns{ true };
    //bool merge_parts_into_layers{ true };
public:
    std::vector<Layer> mLayers;
    LayerManager(const std::filesystem::path& filename)
    {
        auto file = Imf::MultiPartInputFile(filename.string().c_str());
        mLayers = get_all_part_layers(file);
    }

    std::vector<std::string> names()
    {
        std::vector<std::string> names;
        for (auto lyr : mLayers) {
            names.push_back(lyr.display_name());
        }
        return names;
    }

    void set_current(int i)
    {
        assert(("out of range", i < mLayers.size()));
        current_layer_idx = i;
    }

    int current() {
        return current_layer_idx;
    }

    std::string current_name() {
        return mLayers.at(current_layer_idx).display_name();
    }

    std::vector<std::string> current_channel_ids() {
        return mLayers.at(current_layer_idx).channel_ids();
    }

    int current_part_idx() {
        return mLayers.at(current_layer_idx).part_idx();
    }

private:
    static std::vector<Layer> get_all_part_layers(const Imf::MultiPartInputFile& file)
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
};
