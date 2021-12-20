// profile_exr_reading_techs.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <chrono>
#include <filesystem>
namespace fs = std::filesystem;

// OpenImageIO
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebufalgo.h>
using namespace OIIO;

// OpenEXR
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImathBox.h>
#include <OpenEXR/IlmThreadPool.h>
#include <OpenEXR/ImfVersion.h>


#include <fstream>
#include <OpenEXR/ImfStdIO.h>



std::filesystem::path test_folder{ "C:/Users/andris/Desktop/exr_test_images" };

namespace profile {
    std::chrono::time_point<std::chrono::steady_clock> start;
    std::chrono::time_point<std::chrono::steady_clock> stop;
    std::chrono::duration<float> duration;

    void begin(std::string name) {
        std::cout << name << "...";
        start = std::chrono::high_resolution_clock::now();
    }

    void end() {
        stop = std::chrono::high_resolution_clock::now();
        duration = stop - start;
        std::cout << duration << "\n";
    }
}

void read_RED_from_singlelayer() {
    std::filesystem::path filename{ "52_06_EXAM_v06-singe_RGB_color.0002.exr" };

    // setup cache
    ImageCache* cache = ImageCache::create(false /* local cache */);
    //cache->attribute("max_memory_MB", 1024.0f * 16);
    //cache->attribute("autotile", 64);
    //cache->attribute("forcefloat", 1);

    auto img = ImageBuf((test_folder / filename).string(), cache);
    auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 1);
    auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
    static float* data = (float*)malloc(data_size * sizeof(float));
    img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
    free(data);
}

void read_RGB_from_singlelayer() {
    std::filesystem::path filename{"52_06_EXAM_v06-singe_RGB_color.0002.exr"};
    ImageCache* cache = ImageCache::create(false /* local cache */);
    auto img = ImageBuf((test_folder / filename).string(), cache);
    auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 3);
    auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
    static float* data = (float*)malloc(data_size * sizeof(float));
    img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
    free(data);
}

void read_RED_from_multichannel() {
    std::filesystem::path filename{ "multichannel_sequence/52_06_EXAM_v06-vrayraw.0002.exr" };

    ImageCache* cache = ImageCache::create(false /* local cache */);
    auto img = ImageBuf((test_folder / filename).string(), cache);
    auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 1);
    auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
    static float* data = (float*)malloc(data_size * sizeof(float));
    img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
    free(data);
}

void read_RGB_from_multichannel() {
    std::filesystem::path filename{ "multichannel_sequence/52_06_EXAM_v06-vrayraw.0002.exr" };

    ImageCache* cache = ImageCache::create(false /* local cache */);
    auto img = ImageBuf((test_folder / filename).string(), cache);
    auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 3);
    auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
    static float* data = (float*)malloc(data_size * sizeof(float));
    img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
    free(data);
}

void read_RGB_from_multichannel2() {
    std::filesystem::path filename{ "multichannel_sequence/52_06_EXAM_v06-vrayraw.0002.exr" };

    ImageCache* cache = ImageCache::create(false /* local cache */);
    auto img = ImageBuf((test_folder / filename).string(), cache);
    std::vector<int> indices{ 0,1,2 };
    auto layer = OIIO::ImageBufAlgo::channels(img, indices.size(), indices);
    auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 3);
    auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
    static float* data = (float*)malloc(data_size * sizeof(float));
    layer.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
    free(data);
}

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
inline std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;

    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;

    return r;
}

#endif

void read_RGB_from_multichannel_IMF() {
    auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0002.exr");
    

    std::cout <<"exists: " << (std::filesystem::exists(filename) ? "true" : "false") << "\n";

#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
    auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
    auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
    auto inputfile = new Imf::InputFile(*inputStdStream);
#else
    auto inputfile = new Imf_::InputFile(filename.c_str());
#endif
    auto channel_list = inputfile->header().channels();
    std::cout << "version: " << inputfile->version() << "\n";
    std::cout << "tiled: " << Imf::isTiled(inputfile->version()) << "\n";
    std::cout << "multipart: " << Imf::isMultiPart(inputfile->version()) << "\n";
    std::cout << "imf channels: " << "\n";
    for (auto channel = channel_list.begin(); channel!=channel_list.end();++channel) {
        std::cout << "- " << channel.name() << "\n";
    }
}


int main()
{
    std::cout << "Profile reading exr techniques" << "\n";
    /*
    profile::begin("read RED from single channel");
    read_RED_from_singlelayer();
    profile::end();

    profile::begin("read RGB from single channel");
    read_RGB_from_singlelayer();
    profile::end();

    profile::begin("read RED from multichannel");
    read_RED_from_multichannel();
    profile::end();

    profile::begin("read RGB from multichannel");
    read_RGB_from_multichannel();
    profile::end();
    */

    profile::begin("read RGB from multichannel IMF");
    read_RGB_from_multichannel_IMF();
    profile::end();

}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
