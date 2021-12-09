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

#include <windows.h>

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

std::map<std::string, std::vector<int>> group_channels(OIIO::ImageSpec spec) {
    std::map<std::string, std::vector<int>> channel_groups;

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

bool process_file(std::filesystem::path input_file) {
    std::cout << "Read image: " << input_file << "..." << std::endl;
    auto img = OIIO::ImageBuf(input_file.string().c_str());
    auto spec = img.spec();

    std::cout << "List all channels: " << std::endl;
    for (auto i = 0; i < spec.nchannels; i++) {
        std::cout << "- " << "#" << i << " " << spec.channel_name(i) << std::endl;
    }

    std::cout << "Group channels into layers..." << std::endl;
    auto channel_groups = group_channels(img.spec());
    for (auto& [name, indices] : channel_groups) { // print channels
        std::cout << "  " << name << ": ";
        std::vector<std::string> names;
        for (auto i : indices) {
            std::cout << "#" << i << split_string(spec.channel_name(i), ".").back() << ", ";
        }
        std::cout << std::endl;
    }

    auto layers = split_layers(img, channel_groups);

    std::cout << "Write files..." << std::endl;
    write_layers(input_file, layers);

    std::cout << "Done!" << std::endl;
    return true;
}

int run_cmd(int argc, char* argv[]) {
    std::cout << "Parse arguments" << std::endl;

    std::vector<std::filesystem::path> paths;
    for (auto i = 1; i < argc; i++) {
        std::filesystem::path path{ argv[i] };
        if (!std::filesystem::exists(path)) {
            std::cout << "- " << "file does not exists: " << path;
            continue;
        }

        if (!std::filesystem::is_regular_file(path)) {
            std::cout << "- " << "not a file: " << path;
        }

        if (path.extension() != ".exr") {
            std::cout << "- " << "not exr" << path;
            continue;
        }

        paths.push_back(path);
    }

    std::cout << "Process files:" << std::endl;
    for (auto path : paths) {
        process_file(path);
    }

    getchar();
    return EXIT_SUCCESS;
}

int run_gui() {
    
    //std::filesystem::path input_file{ "C:/Users/Bence_4/Desktop/separate_layers_test/52_06_EXAM_v06-vrayraw.0004.exr" };
    std::filesystem::path input_file{ "C:/Users/andris/Downloads/52_06_EXAM_v06-vrayraw.0002.exr" };
    
    // read image
    auto img = OIIO::ImageBuf(input_file.string().c_str());
    auto spec = img.spec();
    //std::cout << spec.serialize(ImageSpec::SerialText) << std::endl;

    auto layers = group_channels(spec);

    // start GUI
    glazy::init();
    while (glazy::is_running()) {
        glazy::new_frame();

        ImGui::Begin("info");
        auto info = spec.serialize(ImageSpec::SerialText);
        ImGui::Text("%s", info.c_str());
        ImGui::End();

        ImGui::ShowDemoWindow();

        ImGui::Begin("layers");
        if (ImGui::BeginTable("table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX))
        {
            for (auto& [name, indices] : layers)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text(name.c_str());
                ImGui::TableNextColumn();

                for (int column = 0; column < 3; column++)
                {
                    
                    for (auto i : indices) {
                        ImGui::Text("#%i %s", i, split_string(spec.channel_name(i), ".").back().c_str());
                        ImGui::SameLine();
                    }
                }
            }
            ImGui::EndTable();
        }

        ImGui::Begin("IO");
        ImGui::Button("Open");
        if (ImGui::BeginTable("table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
            for (auto& [layer_name, indices] : layers) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", layer_name.c_str());
                ImGui::TableNextColumn();

                auto layer_folder = std::filesystem::path(insert_layer_in_path(input_file, layer_name)).parent_path();
                auto layer_file = std::filesystem::path(insert_layer_in_path(input_file, layer_name)).filename();
                ImGui::Text("%s", layer_file.string().c_str());
            }
            ImGui::EndTable();
        }
        ImGui::End();



        ImGui::End();

        //for (auto& [name, indices] : channel_groups) {
        //    std::string indices_text;
        //    for (auto i : indices) {
        //        indices_text += std::to_string(i) + ", ";
        //    }
        //    ImGui::Text("%s: %s", name.c_str(), indices_text.c_str());
        //}

        glazy::end_frame();
    }
    glazy::destroy();

    return EXIT_SUCCESS;
}

int test_process() {
    std::filesystem::path input_path{ "C:/Users/andris/Downloads/52_06_EXAM_v06-vrayraw.0002.exr" };
    process_file(input_path);
    return 0;
}

int main(int argc, char * argv[])
{

    if (argc > 1) {
        return run_cmd(argc, argv);
    }
    else {
        return run_gui();
    }

    getchar();
    return 0;
}

