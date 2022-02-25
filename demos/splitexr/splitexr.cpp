// splitexr.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define VERSION 0.13

#include <iostream>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
using namespace OIIO;

#include <filesystem>
namespace fs = std::filesystem;

#include "stringutils.h"
#include "pathutils.h"

#include <sstream>

#include <ranges>

#include "splitexr.h"

#include <chrono>

std::map<std::string, std::vector<int>> group_channels(const OIIO::ImageSpec& spec) {
    std::map<std::string, std::vector<int>> channel_groups;

    // main channels
    channel_groups["RGB_color"] = { 0,1,2 };
    channel_groups["Alpha"] = {3};
    channel_groups["ZDepth"] = { 4 };

    // AOVs
    for (auto i = 5; i < spec.nchannels; i++) {
        auto channel_name = spec.channelnames[i];

        auto channel_parts = split_string(channel_name, ".");
        
        std::string layer_name{ "" };
        for (auto i = 0; i < channel_parts.size() - 1; i++) {
            layer_name += channel_parts[i];
        }
        if (!channel_groups.contains(layer_name)) {
            channel_groups[layer_name] = std::vector<int>();
        }
        channel_groups[layer_name].push_back(i);
    }
    return channel_groups;
}

bool process_file(const std::filesystem::path& input_file, bool skip_existing=true) {
    std::cout << "Read image" << ": " << input_file << "..." << std::endl;
    auto img = OIIO::ImageBuf(input_file.string().c_str());
    auto spec = img.spec();

    //std::cout << "List all channels: " << std::endl;
    //for (auto i = 0; i < spec.nchannels; i++) {
    //    std::cout << "- " << "#" << i << " " << spec.channel_name(i) << std::endl;
    //}

    std::cout << "Layers found"  << ": " << std::endl;
    auto channel_groups = group_channels(img.spec());
    for (auto const& [name, indices] : channel_groups) { // print channels
        std::cout << "  " << name << ": ";
        std::vector<std::string> names;
        for (auto i : indices) {
            std::cout << "#" << i << "\033[1m" << split_string(spec.channel_name(i), ".").back() << "\033[0m" << ", ";
        }
        std::cout << std::endl;
    }

    /* write each layer */
    std::cout  << "Write layers" << ": " << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    for (auto const& [layer_name, indices] : channel_groups) {
        
        //float channelvalues[] = { 0 /*ignore*/, 0 /*ignore*/, 0 /*ignore*/ };

        // insert layer into filename
        auto folder = input_file.parent_path();
        auto [stem, digits] = split_digits(input_file.stem().string());
        auto extension = input_file.extension();

        auto output_path = join_string({
            folder.string(),
            "/",
            stem,
            layer_name,
            digits.empty() ? "" : ".",
            digits,
            extension.string()
        }, "");

        if (skip_existing && std::filesystem::exists(output_path)) {
            std::cout << output_path << " exists: " << "skipping!" << "\n";
            continue;
        }

        OIIO::ImageBuf layer;
        std::cout << "LAYER NAME: " << layer_name << "\n";
        if (layer_name == "RGB_color") {
            //std::cout << "!!!!!!!!!!!!!!! COLOR" << "\n";
            for (auto i : indices) { std::cout << i; }; std::cout << "\n";
            layer = OIIO::ImageBufAlgo::channels(img, 4, { 0,1,2,3 });
        }
        else if (layer_name == "Alpha") {
            //std::cout << "!!!!!!!!!!!!!!! ALPHA" << "\n";
            for (auto i : indices) { std::cout << i; }; std::cout << "\n";
            layer = OIIO::ImageBufAlgo::channels(img, 4, { 3,3,3,3 }, {}, { "R", "G", "B", "A" }, false, 0);
        }
        else if (layer_name == "ZDepth") {
            //std::cout << "!!!!!!!!!!!!!!! ZDepth" << "\n";
            for (auto i : indices) { std::cout << i; }; std::cout << "\n";
            //OIIO::ImageBufAlgo::channels(src, nchannels, channelorder, channelvalues, newnames, shufflenames, threads);
            layer = OIIO::ImageBufAlgo::channels(img, 4, { 4,4,4,3 }, {}, { "R", "G", "B", "A"}, false, 0);
        }
        else
        {   // AOV
            std::vector<int> channelvalues;
            std::vector<std::string> channelnames;
            for (auto i : indices) {
                channelvalues.push_back(0);
                auto full_name = spec.channel_name(i);
                auto channelname = split_string(full_name, ".").back();
                if (channelname == "X") channelname = "R";
                if (channelname == "Y") channelname = "G";
                if (channelname == "Z") channelname = "B";
                channelnames.push_back(channelname);
            }

            layer = OIIO::ImageBufAlgo::channels(img, indices.size(), indices, {}, OIIO::span(channelnames));
        }



        // write image
        layer.write(output_path);

        std::cout << "- "<< output_path << "\n";
        
    }

    /*
    auto layers = split_layers(img, channel_groups);

    std::cout << "Write files..." << std::endl;
    write_layers(input_file, layers);
    */
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::microseconds>(stop - start);
    std::cout << "Done!" << " [" << duration << "ms]" << std::endl;
    return true;
}

std::map<std::string, std::tuple<int, int>> group_sequences(const std::vector<fs::path> & paths) {
    std::map<std::string, std::tuple<int, int>> sequences;

    for (auto path : paths) {
        
        auto folder = path.parent_path();
        auto stem = path.stem();
        auto ext = path.extension();
        auto [name, digits] = split_digits(stem.string());

        if (digits.empty())
        { // no digits before extension
            sequences[path.string()] = std::tuple<int, int>();
        }
        else
        { // has digits
            std::stringstream ss;
            ss << (folder / name).string();
            for (auto i = 0; i < digits.size(); i++) ss << '#';
            ss << ext.string();
            auto frame_number = std::stoi(digits);
            
            if (!sequences.contains(ss.str())) {
                sequences[ss.str()] = std::tuple<int, int>{frame_number, frame_number};
            }
            else {
                auto& fr = sequences[ss.str()];
                if (std::get<0>(fr) > frame_number) std::get<0>(fr) = frame_number;
                if (std::get<1>(fr) < frame_number) std::get<1>(fr) = frame_number;
            }
        }
    }

    return sequences;
}

std::vector<std::filesystem::path> collect_input_files(const std::vector<std::filesystem::path>& input_paths) {
    // Collect all paths with sequences
    std::vector<std::filesystem::path> results;

    for (auto path : input_paths) {
        if (!std::filesystem::exists(path)) {
            std::cout << "- " << "file does not exists: " << path << "\n";
            continue;
        }

        if (!std::filesystem::is_regular_file(path)) {
            std::cout << "- " << "not a file: " << path << "\n";
        }

        if (path.extension() != ".exr") {
            std::cout << "- " << "not exr" << path << "\n";
            continue;
        }

        results.push_back(path);
    }

    // push sequence items
    for (auto path : results) {
        // find sequence
        auto [pattern, first_frame, last_frame, selected_frame] = scan_for_sequence(path);

        // insert each item to paths
        for (auto F=first_frame; F<=last_frame; F++)
        {
            auto item = sequence_item(pattern, F);
            if (item != path) {
                results.push_back(item);
            }
        }
    }

    // drop duplicates
    std::sort(results.begin(), results.end());
    results.erase(unique(results.begin(), results.end()), results.end());

    return results;
}

void test_collect_files() {
    auto results = collect_input_files({ "C:/Users/andris/Downloads/52_06/52_06_EXAM_v06-vrayraw.0003.exr", "World" });

    auto expected = std::vector<fs::path>{
        "C:/Users/andris/Downloads/52_06/52_06_EXAM_v06-vrayraw.0002.exr",
        "C:/Users/andris/Downloads/52_06/52_06_EXAM_v06-vrayraw.0003.exr"
    };

    std::cout << "test_collect_files: " << (results == expected ? "passed" : "failed") << "\n";
}

void test_processing_files() {
    process_file("C:/Users/andris/Desktop/52_EXAM_v51-raw/52_01_EXAM_v51.0001.exr", false);
}

int main(int argc, char* argv[])
{
    std::cout << "splitexr" << " v" << VERSION << "\n";
    if (argc < 2) {
        std::cout << "drop something!" << std::endl;
        getchar();
        return EXIT_SUCCESS;
    }

    std::vector<fs::path> input_paths;
    for (auto i = 1; i < argc; i++) {
        input_paths.push_back(argv[i]);
    }
    //= {"C:/Users/andris/Downloads/52_06/52_06_EXAM_v06-vrayraw.0003.exr", "World"};

    /* 1. Collect input files */
    std::vector<fs::path> paths = collect_input_files(input_paths);

    /* 2. Print all files */
    std::cout << "files: " << std::endl;
    auto sequences = group_sequences(paths);
    for (auto [key, frame_range] : sequences) {
        std::cout << "- " << key << "[" << std::get<0>(frame_range) << "-" << std::get<1>(frame_range) << "]" << "\n";
    }

    /* 3. Setup cache */
    std::cout << "Set up image cache...";
    ImageCache* cache = ImageCache::create(true /* shared cache */);
    cache->attribute("max_memory_MB", 1024.0f * 16);
    //cache->attribute("autotile", 64);
    cache->attribute("forcefloat", 1);
    std::cout << "done" << "\n";

    /* 4.Process each file */
    std::cout << "Process files:" << std::endl;
    for (auto const& path : paths) {
        process_file(path, true);
    }
    /* Exit */
    std::cout << "finished processing files!" << std::endl << "press any key to exit" << "\n";
    getchar();
    return EXIT_SUCCESS;
}

