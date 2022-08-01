#pragma once

#include "MovieIO.h"
//#include <opencv2/opencv.hpp>

#include <opencv2/videoio/videoio.hpp>

namespace MovieIO
{
    class OpenCVMovieInput : public MovieInput
    {
    private:
        cv::VideoCapture cap;
        int _first_frame;
        int _last_frame;
    public:
        OpenCVMovieInput(std::string name);

        std::tuple<int, int> range() const;

        bool seek(int frame) override;

        Info info() const override;

        bool read(void* data, ChannelSet channel_set = ChannelSet()) override;

        std::vector<ChannelSet> channel_sets() const override;
};
}