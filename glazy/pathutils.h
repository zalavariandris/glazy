#pragma once
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

/*
* Find the image sequence on disk from a single frame
* return pattern such as "image.%04d.tif", start_frame, end_frame, selected frame
* [p, b, e, c]
* 
*/
std::tuple<std::filesystem::path, int, int, int> sequence_from_item(const std::filesystem::path& input_path);
std::filesystem::path sequence_item(const std::filesystem::path& pattern, int F);

/* 
* Find image sequence on disk from sequence pattern
* return pattern such as "image.%04d.tif", start_frame, end_frame, selected frame
* 
* if no item is found, then an empty pattern is returned: { std::filesystem::path(), 0, 0, 0 };
*/
std::tuple<std::filesystem::path, int, int> sequence_from_pattern(std::string sequence_pattern);

/*
return a list of filepaths from a pattrern, and framerange
*/
std::vector<std::filesystem::path> enumerate_sequence(const std::tuple<std::filesystem::path, int, int>& sequence);

std::string to_string(std::vector<std::filesystem::path> sequence);


