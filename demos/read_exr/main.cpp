// read_exr.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include "glazy.h"
#include "imdraw/imdraw.h"
#include "imdraw/imdraw_internal.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/export.h>

inline std::vector<std::string> split_string(std::string text, std::string delimiter) {

    std::vector<std::string> tokens;

    size_t pos = 0;
    while ((pos = text.find(delimiter)) != std::string::npos) {
        auto token = text.substr(0, pos);
        tokens.push_back(token);
        text.erase(0, pos + delimiter.length());
    }
    tokens.push_back(text);
    return tokens;
};

inline std::string join_string(std::vector<std::string> tokens) {
    std::string text;
    for (auto token : tokens) {
        text += token;
    }
    return text;
}

inline std::tuple<std::string, std::string> split_digits(std::string stem) {
    int digits_count = 0;
    while (std::isdigit(stem[stem.size() - 1 - digits_count])) {
        digits_count++;
    }

    auto text = stem.substr(0, stem.size() - digits_count);
    auto digits = stem.substr(stem.size() - digits_count, -1);

    return { text, digits };
}

inline std::string insert_layer_in_path(std::filesystem::path path, std::string layer_name) {
    auto [stem, digits] = split_digits(path.stem().string());
    auto layer_path = join_string({
        path.parent_path().string(),
        "/",
        stem,
        digits,
        digits.empty() ? "" : ".",
        layer_name,
        path.extension().string()
        });
    return layer_path;
}

std::map<std::string, OIIO::ImageBuf> seperate_layers(const OIIO::ImageBuf& img) {
    std::map<std::string, OIIO::ImageBuf> layers;
    auto spec = img.spec();
    std::cout << "separate layers..." << std::endl;
    for (auto i = 0; i < spec.nchannels; i++) {
        auto channel_name = spec.channelnames[i];
        std::cout << "#" << i << channel_name << std::endl;
        layers[channel_name] = OIIO::ImageBufAlgo::channels(img, 1, { i });;
    }
    return layers;
}

std::map<std::string, std::vector<int>> group_channels(OIIO::ImageBuf img) {
    std::map<std::string, std::vector<int>> channel_groups;
    auto spec = img.spec();

    // main channels
    channel_groups["RGB_color"] = { 0,1,2 };
    channel_groups["Alpha"] = { 3 };
    channel_groups["ZDepth"] = { 4 };

    // AOVs
    for (auto i = 5; i < spec.nchannels; i++) {
        auto channel_name = spec.channelnames[i];

        auto channel_array = split_string(channel_name, ".");

        auto layer_name = channel_array.front();
        if (!channel_groups.contains(layer_name)) {
            channel_groups[layer_name] = std::vector<int>();
        }
        channel_groups[layer_name].push_back(i);
    }
    return channel_groups;
}

std::map<std::string, OIIO::ImageBuf> split_layers(const OIIO::ImageBuf& img, std::map<std::string, std::vector<int>> channel_groups) {
    std::cout << "Split layers..." << std::endl;
    std::map<std::string, OIIO::ImageBuf> layers;
    for (auto& [layer_name, indices] : channel_groups) {
        std::cout << "  " << layer_name << std::endl;
        layers[layer_name] = OIIO::ImageBufAlgo::channels(img, indices.size(), indices);
    }

    return layers;
}

void write_layers(std::filesystem::path output_path, std::map<std::string, OIIO::ImageBuf> layers) {
    for (auto& [layer_name, img] : layers) {
        // insert layer into filename
        auto folder = output_path.parent_path();
        auto [stem, digits] = split_digits(output_path.stem().string());
        auto extension = output_path.extension();

        auto output_path = join_string({
            folder.string(),
            "/",
            stem,
            layer_name,
            digits.empty() ? "" : ".",
            digits,
            extension.string()
            });
        std::cout << "  " << output_path << std::endl;

        // write image
        img.write(output_path);
    }
}

using namespace OIIO;
int main()
{
    std::filesystem::path input_path{ "C:/Users/Bence_4/Desktop/separate_layers_test/52_06_EXAM_v06-vrayraw.0004.exr" };
    
    std::cout << "Read image..." << std::endl;
    auto img = OIIO::ImageBuf(input_path.string().c_str());
    auto spec = img.spec();

    std::cout << "Group channels into layers..." << std::endl;
    auto channel_groups = group_channels(img);
    for (auto& [name, indices] : channel_groups) { // print channels
        std::cout << "  " << name << ": ";
        for (auto i : indices) {
            std::cout << split_string(spec.channelnames[i], ".").back() << ", ";
        }
        std::cout << std::endl;
    }

    auto layers = split_layers(img, channel_groups);

    std::cout << "Write files..." << std::endl;
    write_layers(input_path, layers);

    // start GUI
    glazy::init();
    while (glazy::is_running()) {
        glazy::new_frame();

        for (auto &[name, indices] : channel_groups) {
            std::string indices_text;
            for (auto i : indices) {
                indices_text += std::to_string(i) + ", ";
            }
            ImGui::Text("%s: %s", name.c_str(), indices_text.c_str());
        }

        glazy::end_frame();
    }
    glazy::destroy();

    std::cout << "end" << std::endl;
    getchar();
}

