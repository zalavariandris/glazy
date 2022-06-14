#include "FileSequence.h"
#include "pathutils.h"


FileSequence::FileSequence(std::filesystem::path filepath) {
    auto [format, begin, end, current] = scan_for_sequence(filepath);
    _pattern = format.string();
    first_frame = begin;
    last_frame = end;
}

std::string FileSequence::pattern() const {
    return _pattern;
}

std::filesystem::path FileSequence::item(int F) const
{
    if (_pattern.empty()) return std::filesystem::path();

    /// format pattern with framenumber
    // ref: educative.io educative.io/edpresso/how-to-use-the-sprintf-method-in-c
    int size_s = std::snprintf(nullptr, 0, _pattern.c_str(), F) + 1; // Extra space for '\0'
    if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, _pattern.c_str(), F);
    auto filename = std::string(buf.get(), buf.get() + size - 1);

    // return filepath
    return std::filesystem::path(filename);
}

std::vector<int> FileSequence::missing_frames() const
{
    std::vector<int> missing_frames;
    for (int F = first_frame; F <= last_frame; F++) {
        auto framepath = this->item(F);
        bool exists = std::filesystem::exists(framepath);
        if (!exists)
        {
            missing_frames.push_back(F);
        }
    }
    return missing_frames;
}