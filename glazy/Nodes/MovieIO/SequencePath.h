#pragma once

#include <filesystem>
#include <string>

namespace filesystem
{
    class SequencePath
    {
    private:
        std::string _pattern;
        int _first_frame;
        int _last_frame;
    public:
        /// pattern
        ///  
        SequencePath(std::string sequence_pattern, std::tuple<int, int> range);

        // create sequence path from item on disk
        static SequencePath from_item_on_disk(std::filesystem::path item);

        bool exists();

        std::filesystem::path at(int frame);

        std::tuple<int, int> range();

        std::vector<int> missing_frames();
    };
}