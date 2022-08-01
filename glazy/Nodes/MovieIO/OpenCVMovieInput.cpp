#include "OpenCVMovieInput.h"

std::string cvtype2str(int type) {
    std::string r;

    uchar depth = type & CV_MAT_DEPTH_MASK;
    uchar chans = 1 + (type >> CV_CN_SHIFT);

    switch (depth) {
    case CV_8U:  r = "8U"; break;
    case CV_8S:  r = "8S"; break;
    case CV_16U: r = "16U"; break;
    case CV_16S: r = "16S"; break;
    case CV_32S: r = "32S"; break;
    case CV_32F: r = "32F"; break;
    case CV_64F: r = "64F"; break;
    default:     r = "User"; break;
    }

    r += "C";
    r += (chans + '0');

    return r;
}

namespace MovieIO
{
    OpenCVMovieInput::OpenCVMovieInput(std::string name)
    {
        cap = cv::VideoCapture(name);

        //cv::Mat image;
        //cap.read(image);
        //auto tname = cvtype2str(image.type());

        _first_frame = 0;
        _last_frame = cap.get(cv::CAP_PROP_FRAME_COUNT);
    }

    std::tuple<int, int> OpenCVMovieInput::range() const
    {
        return { _first_frame, _last_frame };
    }

    bool OpenCVMovieInput::seek(int frame)
    {
        if (cap.get(cv::CAP_PROP_POS_FRAMES) != frame)
        {
            cap.set(cv::CAP_PROP_POS_FRAMES, frame);
        }
        return true;
    }

    Info OpenCVMovieInput::info() const
    {
        return {
            0,0,
            (int)cap.get(cv::CAP_PROP_FRAME_WIDTH),
            (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT),
            {"B", "G", "R"},
            OIIO::TypeUInt8
        };
    }

    bool OpenCVMovieInput::read(void* data, ChannelSet channel_set)
    {
        auto spec = info();

        cv::Mat mat(spec.height, spec.width, CV_8UC3, data);
        bool success = cap.read(mat);
        //cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
        return success;
    }

    std::vector<ChannelSet> OpenCVMovieInput::channel_sets() const
    {
        return { ChannelSet("color", 0, { "R", "G", "B" }) };
    }
}