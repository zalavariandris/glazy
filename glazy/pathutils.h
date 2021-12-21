#pragma once
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

std::vector<std::filesystem::path> find_sequence(std::filesystem::path input_path);

std::string to_string(std::vector<std::filesystem::path> sequence);