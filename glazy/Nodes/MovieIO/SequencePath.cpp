#include "SequencePath.h"
#include <regex>
#include <assert.h>

namespace filesystem
{
    SequencePath::SequencePath(std::string sequence_pattern, std::tuple<int, int> range)
    {
        static std::regex sequence_match_re("(.+)%0([0-9]+)d(.+)");
        std::smatch sequence_groups;
        if (!std::regex_match(sequence_pattern, sequence_groups, sequence_match_re)) {
            throw "printf pattern not recognized eg.: filename.%04d.exr";
        }

        _pattern = sequence_pattern;
        _first_frame = std::get<0>(range);
        _last_frame = std::get<1>(range);
    }

    // create sequence path from item on disk
    SequencePath SequencePath::from_item_on_disk(std::filesystem::path item)
    {
        assert(std::filesystem::exists(item));
    }

    bool SequencePath::exists() {
        auto first_path = at(_first_frame);
        auto last_path = at(_last_frame);

        return std::filesystem::exists(first_path) && std::filesystem::exists(last_path);
    }

    std::filesystem::path SequencePath::at(int frame)
    {
        /// this file does not necesseraly exists.

        // ref: educative.io educative.io/edpresso/how-to-use-the-sprintf-method-in-c
        int size_s = std::snprintf(nullptr, 0, _pattern.c_str(), frame) + 1; // Extra space for '\0'
        if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
        auto size = static_cast<size_t>(size_s);
        auto buf = std::make_unique<char[]>(size);
        std::snprintf(buf.get(), size, _pattern.c_str(), frame);

        auto filename = std::string(buf.get(), buf.get() + size - 1);
        return std::filesystem::path(filename);
    }

    std::tuple<int, int> SequencePath::range()
    {
        return { _first_frame, _last_frame };
    }

    std::vector<int> SequencePath::missing_frames()
    {
        std::vector<int> missing;
        for (int frame = _first_frame; frame <= _last_frame; frame++)
        {
            if (!std::filesystem::exists(at(frame)))
            {
                missing.push_back(frame);
            }
        }
        return missing;
    }
}