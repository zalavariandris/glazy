#include "pathutils.h"

#include "stringutils.h"
#include <cassert>
#include <cstdio> // printf sprintf snprintf

std::string to_string(std::vector<std::filesystem::path> sequence) {
    // collect frame numbers
    std::cout << "Sequence items:" << std::endl;
    std::vector<int> frame_numbers;
    for (const auto& seq_item : sequence) {
        const auto& [name, digits] = split_digits(seq_item.stem().string());
        frame_numbers.push_back(std::stoi(digits));
    }
    sort(frame_numbers.begin(), frame_numbers.end());

    // format sequence to human readable string
    int first_frame = frame_numbers[0];
    int last_frame = frame_numbers.back();

    const auto& [input_name, input_digits] = split_digits(sequence[0].stem().string());
    std::string text = input_name;
    for (auto i = 0; i < input_digits.size(); i++) {
        text += "#";
    }
    text += sequence[0].extension().string();
    text += " [" + std::to_string(first_frame) + "-" + std::to_string(last_frame) + "]";
    return text;
}

std::tuple<std::filesystem::path, int, int, int> scan_for_sequence(const std::filesystem::path& input_path) {
    assert(std::filesystem::exists(input_path));

    std::vector<std::filesystem::path> sequence;
    std::vector<int> framenumbers;

    // find sequence item in folder
    const auto& [input_name, input_digits] = split_digits(input_path.stem().string());
    if (input_digits.empty()) {
        return {input_path, 0, 0, 0};
    }
    int selected_frame = std::stoi(input_digits);
    const auto& input_folder = input_path.parent_path();
    const std::filesystem::path sequence_pattern = input_folder / (input_name +"%0" + std::to_string(input_digits.size()) + "d" + input_path.extension().string());

    for (auto&& entry : std::filesystem::directory_iterator{ input_folder, std::filesystem::directory_options::skip_permission_denied }) {
        try {
            // match filename and digits count
            const auto& [name, digits] = split_digits(entry.path().stem().string());
            bool IsSequenceItem = (name == input_name) && (digits.size() == input_digits.size());
            //std::cout << "check file " << IsSequenceItem << " " << path << std::endl;
            //std::cout << name << " <> " << input_name << std::endl;
            if (IsSequenceItem) {
                sequence.push_back(entry.path());
                framenumbers.push_back(std::stoi(digits));
            }
        }
        catch (const std::system_error& ex) {
            std::cout << "Exception: " << ex.what() << "\n-  probably std::filesystem does not support Unicode filenames" << std::endl;
        }
    }

    std::sort(framenumbers.begin(), framenumbers.end());
    return { sequence_pattern, framenumbers[0], framenumbers.back(), selected_frame};
}

std::filesystem::path sequence_item(const std::filesystem::path& pattern, int F) {
    if (pattern.empty()) return std::filesystem::path();

    // copy from educative.io educative.io/edpresso/how-to-use-the-sprintf-method-in-c
    int size_s = std::snprintf(nullptr, 0, pattern.string().c_str(), F) + 1; // Extra space for '\0'
    if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, pattern.string().c_str(), F);
    return std::filesystem::path(std::string(buf.get(), buf.get() + size - 1));
}