#pragma once

#include <vector>
#include <filesystem>

class Layer {
private:
    std::string mName;
    std::vector<std::string> mChannels;

    std::string mPart_name;
    int mPart_idx;
    std::vector<std::string> full_channel_name;
public:
    Layer(std::string name, std::vector<std::string> channels, std::string part_name, int part_idx) : mName(name), mChannels(channels), mPart_name(part_name), mPart_idx(part_idx) {}

    std::string part_name() const;

    int part_idx() const;

    /// the layer name
    std::string name() const;

    std::string path() const;

    std::vector<std::string> channel_ids() const; /// return all channels with the full EXR channel names
    std::vector<std::string> channels() const; // return all the channels
    std::string display_name() const; /// nice name

    /// Create a new Layer from requested channels on this Layer
    Layer sublayer(std::vector<std::string> subchannels, std::string sublayer_name = "", bool compose = true) const;

    static std::tuple<std::string, std::string> split_channel_id(const std::string& channel_id);

    std::vector<Layer> split_by_delimiter() const;

    std::vector<Layer> split_by_patterns(const std::vector<std::vector<std::string>>& patterns) const;

    /// support stringtream
    friend auto operator<<(std::ostream& os, Layer const& lyr) -> std::ostream& {
        os << lyr.display_name();
        return os;
    }
};

class EXRLayerManager
{
    int current_layer_idx = 0;
    std::vector<Layer> mLayers;
    //bool group_by_patterns{ true };
    //bool merge_parts_into_layers{ true };
public:
    
    EXRLayerManager(const std::filesystem::path& filename);

    std::vector<std::string> names();

    void set_current(int i);

    int current();

    std::string current_name();

    std::vector<std::string> current_channel_ids();

    int current_part_idx();

    std::vector<Layer> layers() {
        return mLayers;
    }
};
