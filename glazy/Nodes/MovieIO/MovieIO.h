#pragma once

#include "OpenImageIO/typedesc.h"


namespace MovieIO {
    struct Info {
        int x;
        int y;
        int width;
        int height;
        std::vector<std::string> channels;
        OIIO::TypeDesc format;
    };

    struct ChannelSet {
        std::string name;
        int part;
        std::vector<std::string> channels;

        ChannelSet() :name("color"), part(0), channels({ "R", "G", "B", "A" }){};

        ChannelSet(std::string name, int part, std::vector<std::string> channels) : 
            name(name), part(part), channels(channels){}
    };

    class MovieInput {
    public:
        static std::unique_ptr<MovieInput> open(const std::string& name);

        virtual std::tuple<int, int> range() const = 0;

        virtual bool seek(int frame) = 0;

        virtual Info info() const = 0;

        virtual bool read(void* data, ChannelSet channel_set=ChannelSet()) = 0;

        virtual std::vector<ChannelSet> channel_sets() const = 0;
    };
}