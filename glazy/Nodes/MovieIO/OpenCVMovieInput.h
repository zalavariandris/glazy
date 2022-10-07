#pragma once

#include "MovieIO.h"
//#include <opencv2/opencv.hpp>

#include <opencv2/videoio/videoio.hpp>

namespace MovieIO
{
    class OpenCVMovieInput : public MovieInput
    {
    private:
        mutable cv::VideoCapture cap;
        int _first_frame;
        int _last_frame;
    public:
        OpenCVMovieInput(std::string name);

        std::tuple<int, int> range() const;

        bool seek(int frame, ChannelSet channel_set) override;

        Info info() const override;

        bool read(void* data) const override;

        std::vector<ChannelSet> channel_sets() const override;
};
}