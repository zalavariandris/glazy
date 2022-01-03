#pragma once
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

std::vector<std::filesystem::path> find_sequence(std::filesystem::path input_path, int* start_frame = NULL, int* end_frame = NULL);

/*
* Find the image sequence on disk from a single frame
* return pattern such as "image.%04d.tif", start_frame, and_frame
*/
std::tuple<std::filesystem::path, int, int> scan_sequence(const std::filesystem::path& input_path);
std::filesystem::path sequence_item(const std::filesystem::path& pattern, int F);

/*
return a list of filepaths from a pattrern, and framerange
*/
std::vector<std::filesystem::path> enumerate_sequence(const std::tuple<std::filesystem::path, int, int>& sequence);

std::string to_string(std::vector<std::filesystem::path> sequence);