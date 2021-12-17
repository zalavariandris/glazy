#pragma once
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "stringutils.h"

std::vector<std::filesystem::path> find_sequence(std::filesystem::path input_path) {
    assert(std::filesystem::exists(input_path));

    std::vector<std::filesystem::path> sequence;

    // find sequence item in folder
    auto [input_name, input_digits] = split_digits(input_path.stem().string());
    auto folder = input_path.parent_path();

    for (std::filesystem::path path : std::filesystem::directory_iterator{ folder, std::filesystem::directory_options::skip_permission_denied }) {
        try {
            // match filename and digits count
            auto [name, digits] = split_digits(path.stem().string());
            bool IsSequenceItem = (name == input_name) && (digits.size() == input_digits.size());
            //std::cout << "check file " << IsSequenceItem << " " << path << std::endl;
            //std::cout << name << " <> " << input_name << std::endl;
            if (IsSequenceItem) {
                sequence.push_back(path);
            }
        }
        catch (const std::system_error& ex) {
            std::cout << "Exception: " << ex.what() << "\n-  probably std::filesystem does not support Unicode filenames" << std::endl;
        }
    }


    return sequence;
}

std::string to_string(std::vector<std::filesystem::path> sequence) {
    // collect frame numbers
    std::cout << "Sequence items:" << std::endl;
    std::vector<int> frame_numbers;
    for (auto seq_item : sequence) {
        auto [name, digits] = split_digits(seq_item.stem().string());
        frame_numbers.push_back(std::stoi(digits));
    }
    sort(frame_numbers.begin(), frame_numbers.end());

    // format sequence to human readable string
    int first_frame = frame_numbers[0];
    int last_frame = frame_numbers.back();

    auto [input_name, input_digits] = split_digits(sequence[0].stem().string());
    std::string text = input_name;
    for (auto i = 0; i < input_digits.size(); i++) {
        text += "#";
    }
    text += sequence[0].extension().string();
    text += " [" + std::to_string(first_frame) + "-" + std::to_string(last_frame) + "]";
    return text;
}