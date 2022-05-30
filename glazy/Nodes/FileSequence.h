#pragma once
#include <filesystem>

class FileSequence
{
public:
    FileSequence()
    {
        pattern = "";
        first_frame = 0;
        last_frame = 0;
    }

    FileSequence(std::filesystem::path filepath);

    std::filesystem::path item(int F) const;

    std::filesystem::path pattern;
    int first_frame;
    int last_frame;
    int length() const{
        return last_frame - first_frame + 1;
    }

    std::vector<int> missing_frames() const;
};