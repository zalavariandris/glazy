#pragma once
#include <filesystem>
#include <string>

class FileSequence
{
public:
    FileSequence()
    {
        _pattern = "";
        first_frame = 0;
        last_frame = 0;
    }

    FileSequence(std::filesystem::path filepath);

    std::string pattern() const;
    std::filesystem::path item(int F) const;

    
    int first_frame;
    int last_frame;
    int length() const{
        return last_frame - first_frame + 1;
    }

    std::vector<int> missing_frames() const;

private:
    std::string _pattern;
};