#pragma once

#include "OpenImageIO/typedesc.h"
#include <iostream>

namespace MovieIO {
    struct Info {
        int x;
        int y;
        int width;
        int height;
        std::vector<std::string> channels;
        OIIO::TypeDesc format;
    };

    
    inline std::ostream& operator << (std::ostream& o, const Info& info)
    {
        o << info.width <<"x" << info.height;
        o << " ";
        for (auto channel : info.channels)
        {
            o << channel << " ";
        }
        o << " ";
        o << info.format;
        return o;
    }

    struct ChannelSet {
        std::string name;
        int part;
        std::vector<std::string> channels;

        ChannelSet() :name("color"), part(0), channels({ "R", "G", "B", "A" }){};

        ChannelSet(std::string name, int part, std::vector<std::string> channels) :
            name(name), part(part), channels(channels) {};

        bool operator==(const ChannelSet& rhs)
        {
            return channels == rhs.channels && part == rhs.part;
        }
    };

    class MovieInput {
    public:
        static std::unique_ptr<MovieInput> open(const std::string& name);

        virtual std::tuple<int, int> range() const = 0;

        virtual bool seek(int frame, ChannelSet channel_set) = 0;

        virtual Info info() const = 0;

        virtual bool read(void* data) const = 0;

        virtual std::vector<ChannelSet> channel_sets() const = 0;
    };
}