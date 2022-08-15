#pragma once

#include <string>
#include <tuple>
#include <vector>
#include <map>
#include <unordered_map>
#include "../MovieIO/MovieIO.h"
#include "nodes.h"

#include "MemoryImage.h"

struct CacheKey {
    int frame;

    bool operator==(const CacheKey& other) const {
        return frame == other.frame;
    }
};

namespace std
{
    template<>
    struct hash<CacheKey>
    {
        std::size_t operator()(const CacheKey& k) const
        {
            return std::hash<int>()(k.frame);
        }
    };
}

class ReadNode
{
private:
    std::string _cache_pattern;

    //using MemoryImage = std::tuple<void*, int, int, std::vector<std::string>, OIIO::TypeDesc>; //ptr, width, height, channels, format

    int _first_frame;
    int _last_frame;

    std::unique_ptr<MovieIO::MovieInput> _movie_input;


    std::unordered_map<CacheKey, std::shared_ptr<MemoryImage>> _cache; // memory, width, height, channels

public:
    Nodes::Attribute<std::string> file; // filename or sequence pattern
    Nodes::Attribute<int> frame;
    Nodes::Attribute<int> selected_channelset_idx;
    Nodes::Attribute<bool> enable_cache{ true };
    Nodes::Outlet<std::shared_ptr<MemoryImage>> plate_out;

    size_t capacity{ 0 };


    int first_frame() const {
        return _first_frame;
    }

    int last_frame() const {
        return _last_frame;
    }

    int length() const {
        return _last_frame - _first_frame + 1;
    }

    ReadNode();

    void read();

    std::vector<std::tuple<int, int>> cached_range() const;

    void onGUI();
};