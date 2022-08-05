#pragma once

#include "MovieIO.h"
#include "OpenImageIO/imageio.h"
#include <filesystem>

namespace MovieIO {
    class OIIOMovieInput : public MovieInput
    {
        std::unique_ptr<OIIO::ImageInput> _current_input;
        std::filesystem::path _current_filepath;
        std::string _sequence_path;
        bool _is_sequence{ false };
        int _current_frame;
        std::vector<ChannelSet> _channel_sets;

        std::map<std::string, int> channel_idx_by_name;
    public:
        int _first_frame;
        int _last_frame;

        // accepts an existing image file, or printf pattern file sequence
        // eg folder/filename.0007.exr -> single frame
        //    folder/filename.%04d.exr -> sequence of images on disk
        OIIOMovieInput(std::string sequence_path);

        void update_channel_sets();

        std::tuple<int, int> range() const override;

        bool seek(int frame) override;

        Info info() const override;

        bool read(void* data, ChannelSet channel_set = ChannelSet()) override;

        std::vector<ChannelSet> channel_sets() const override;
    };
}