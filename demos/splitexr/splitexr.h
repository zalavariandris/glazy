
#include <vector>
#include <filesystem>

std::vector<std::filesystem::path> collect_input_files(const std::vector<std::filesystem::path>& input_paths);
bool process_file(const std::filesystem::path& input_file);