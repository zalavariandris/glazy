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

//#include <OpenEXR/ImfInputFile.h>
//#include <OpenEXR/ImfGenericInputFile.h> // Imf::InputFile


#include <OpenEXR/half.h> // <half> type

#include <OpenEXR/ImfArray.h> //Imf::Array2D

#include <fstream>
#include <OpenEXR/ImfStdIO.h>

#include <cassert>



std::filesystem::path test_folder{ "C:/Users/andris/Desktop/testimages/exr_test_images" };

namespace profile {
    std::chrono::time_point<std::chrono::steady_clock> start;
    std::chrono::time_point<std::chrono::steady_clock> stop;
    std::chrono::duration<float> duration;

    std::string current_name="";
    std::map<std::string, std::chrono::duration<float>> times;

    void begin(std::string name) {
        assert(current_name == "");
        current_name = name;
        std::cout << name << "...";
        start = std::chrono::high_resolution_clock::now();
    }

    void end() {
        stop = std::chrono::high_resolution_clock::now();
        duration = stop - start;
        std::cout << duration << "\n";
        times[current_name] = duration;
        current_name = "";
    }
}

namespace With_OIIO_ImageBuffer {
    void read_RED_from_singlelayer() {
        auto filename = test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr";
        assert(("file does not exist", fs::exists(filename)));

        // setup cache
        ImageCache* cache = ImageCache::create(false /* local cache */);
        //cache->attribute("max_memory_MB", 1024.0f * 16);
        //cache->attribute("autotile", 64);
        //cache->attribute("forcefloat", 1);

        auto img = ImageBuf(filename.string(), cache);
        auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 1);
        auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
        float* data = (float*)malloc(data_size * sizeof(float));
        img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
        free(data);
    }

    void read_RGB_from_singlelayer() {
        auto filename = test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr";
        assert(("file does not exist", fs::exists(filename)));

        ImageCache* cache = ImageCache::create(false /* local cache */);

        auto img = ImageBuf(filename.string(), cache);
        auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 3);
        auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
        static float* data = (float*)malloc(data_size * sizeof(float));
        img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
        free(data);
    }

    void read_RED_from_multichannel() {
        auto filename = test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr";
        assert(("file does not exist", fs::exists(filename)));

        ImageCache* cache = ImageCache::create(false /* local cache */);

        auto img = ImageBuf(filename.string(), cache);
        auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 1);
        auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
        static float* data = (float*)malloc(data_size * sizeof(float));
        img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
        free(data);
    }

    void read_RGB_from_multichannel(ImageCache* cache) {
        auto filename = test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr";
        assert(("file does not exist", fs::exists(filename)));

        auto img = ImageBuf(filename.string(), cache);
        auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 3);
        auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
        float* data = (float*)malloc(data_size * sizeof(float));
        img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
        free(data);
    }

    void read_RGB_from_multichannel2(ImageCache * cache) {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        assert(("file does not exist", fs::exists(filename)));

        auto img = ImageBuf((test_folder / filename).string(), cache);
        std::vector<int> indices{ 0,1,2 };
        auto layer = OIIO::ImageBufAlgo::channels(img, indices.size(), indices);
        auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 3);
        auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
        float* data = (float*)malloc(data_size * sizeof(float));
        layer.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
        free(data);
    }

    void read_RGB_from_multichannel3(ImageCache* cache) {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        assert(("file does not exist", fs::exists(filename)));

        auto img = ImageBuf(filename.string(), cache);
        //img.threads(32);

        int chbegin = 0;
        int chend = 4;
        auto success = img.read(0, 0, chbegin, chend, false, TypeDesc::FLOAT);
        auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 3);

        auto data_size = roi_rgb.width() * roi_rgb.height() * roi_rgb.nchannels();
        float* data = (float*)malloc(data_size * sizeof(float));
        img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data, AutoStride, AutoStride, AutoStride);
        free(data);
    }

    void read_RGB_from_multichannel_USE_CACHE_WHEN_AVAILABLE(ImageCache* cache) {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        assert(("file does not exist", fs::exists(filename)));
        auto img = ImageBuf(filename.string(), cache);
        std::vector<int> indices{ 0,1,2 };

        auto roi_rgb = ROI(0, img.spec().width, 0, img.spec().height, 0, 1, 0, 2);
        auto data_size = roi_rgb.width() * roi_rgb.height() * 3;
        float* data = (float*)malloc(data_size * sizeof(float));

        assert(("when profiling ImageBuffer supposed to backed up with image cache", img.cachedpixels()));
        if (img.cachedpixels()) {
            std::cout << "(gettin pixels from image cache)" << "\n";
            cache->get_pixels(OIIO::ustring(img.name()), 0, 0, 0, img.spec().width, 0, img.spec().height, 0, 1, 0, 3, TypeDesc::FLOAT, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, 0, 3);
        }
        else {
            img.get_pixels(roi_rgb, TypeDesc::TypeFloat, data);
        }

        free(data);
    }
}

namespace With_OIIO_ImageCache{
    void read_RGB_from_multichannel(ImageCache* cache) {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        
        assert(("file does not exist", fs::exists(filename)));
        ImageSpec spec;
        cache->get_imagespec(OIIO::ustring(filename.string()), spec);
        float* data = (float*)malloc(spec.width*spec.height*3 * sizeof(float));
        cache->get_pixels(OIIO::ustring(filename.string()), 0, 0, 0, spec.width, 0, spec.height, 0, 1, 0, 3, TypeDesc::FLOAT, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, 0, 3);


        free(data);
    }
}

namespace With_OIIO_ImageInput {
    void read_ALL_from_multichannel() {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        assert(("file does not exist", fs::exists(filename)));

        auto in = ImageInput::open(filename.string());
        if (!in)
            return;
        const ImageSpec& spec = in->spec();
        int xres = spec.width;
        int yres = spec.height;
        int channels = spec.nchannels;
        std::vector<float> pixels(xres * yres * channels);
        in->read_image(TypeDesc::FLOAT, &pixels[0]);
        in->close();
    }

    void read_RGB_from_multichannel() {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        assert(("file does not exist", fs::exists(filename)));

        auto in = ImageInput::open(filename.string());
        if (!in)
            return;
        const ImageSpec& spec = in->spec();
        int xres = spec.width;
        int yres = spec.height;
        int channels = spec.nchannels;
        std::vector<float> pixels(xres * yres * 3);
        in->read_image(0, 2, TypeDesc::FLOAT, &pixels[0]);
        in->close();
    }

    void read_RGB_from_multichannel_carray() {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        assert(("file does not exist", fs::exists(filename)));

        auto in = ImageInput::open(filename.string());
        if (!in)
            return;
        const ImageSpec& spec = in->spec();
        int xres = spec.width;
        int yres = spec.height;
        int channels = spec.nchannels;
        float* pixels = (float*)malloc(xres*yres*3 * sizeof(float));
        in->read_image(0, 2, TypeDesc::FLOAT, pixels);
        in->close();
        free(pixels);
    }
}

namespace With_OpenEXR {
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

    void read_RGB_from_multichannel(
        Imf::Array2D<float>& rPixels,
        Imf::Array2D<float>& gPixels,
        Imf::Array2D<float>& bPixels, bool verbose = false)
    {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        assert(("file does not exist", fs::exists(filename)));

#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
        auto file = new Imf::InputFile(*inputStdStream);
#else
        auto file = new Imf::InputFile(filename.c_str());
#endif

        auto dw = file->header().dataWindow();
        auto width = dw.max.x - dw.min.x + 1;
        auto height = dw.max.y - dw.min.y + 1;

        // define memory layout
        rPixels.resizeErase(height, width);
        gPixels.resizeErase(height, width);
        bPixels.resizeErase(height, width);

        Imf::FrameBuffer frameBuffer;

        frameBuffer.insert("R",                                    // name
            Imf::Slice(Imf::FLOAT,                            // type
                (char*)(&rPixels[0][0] -      // base
                    dw.min.x -
                    dw.min.y * width),
                sizeof(rPixels[0][0]) * 1,     // xStride
                sizeof(rPixels[0][0]) * width, // yStride
                1, 1,                            // x/y sampling
                0.0));                           // fillValue

        frameBuffer.insert("G",                                    // name
            Imf::Slice(Imf::FLOAT,                            // type
                (char*)(&gPixels[0][0] -      // base
                    dw.min.x -
                    dw.min.y * width),
                sizeof(gPixels[0][0]) * 1,     // xStride
                sizeof(gPixels[0][0]) * width, // yStride
                1, 1,                            // x/y sampling
                0.0));                           // fillValue

        frameBuffer.insert("B",                                    // name
            Imf::Slice(Imf::FLOAT,                           // type
                (char*)(&bPixels[0][0] -      // base
                    dw.min.x -
                    dw.min.y * width),
                sizeof(bPixels[0][0]) * 1,     // xStride
                sizeof(bPixels[0][0]) * width, // yStride
                1, 1,                            // x/y sampling
                FLT_MAX));                       // fillValue


        // read pixels
        file->setFrameBuffer(frameBuffer);
        file->readPixels(dw.min.y, dw.max.y);
    }

    typedef struct RGB {
        float r;
        float g;
        float b;
    };

    void print_header(const Imf::InputFile& file) {
        std::cout << "version: " << file.version() << "\n";
        std::cout << "tiled: " << Imf::isTiled(file.version()) << "\n";
        std::cout << "multipart: " << Imf::isMultiPart(file.version()) << "\n";

        // print channels
        auto channel_list = file.header().channels();
        std::cout << "channels: " << "\n";
        for (auto channel = channel_list.begin(); channel != channel_list.end(); ++channel) {
            std::cout << channel.name() << ", ";
        }
        std::cout << "\n";

        // print layers and corresponding channels
        std::set<std::string> layers;
        channel_list.layers(layers);
        std::cout << "layers: " << "\n";
        for (auto layer : layers) {
            std::cout << "- " << layer << "\n";

            Imf::ChannelList::ConstIterator first, last;
            channel_list.channelsInLayer(layer, first, last);
            for (auto c = first; c != last; ++c) {
                std::cout << "    ch: " << c.name() << "\n";
            }
        }
    }

    void read_RGB_from_multichannel_interleaved(
        Imf::Array2D<RGB>& pixels)
    {
        auto filename = (test_folder / "multichannel_sequence/52_06_EXAM_v06-vrayraw.0010.exr");
        assert(("file does not exist", fs::exists(filename)));

#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
        auto file = new Imf::InputFile(*inputStdStream);
#else
        auto file = new Imf::InputFile(filename.c_str());
#endif

        auto dw = file->header().dataWindow();
        auto width = dw.max.x - dw.min.x + 1;
        auto height = dw.max.y - dw.min.y + 1;
        int dx = dw.min.x;
        int dy = dw.min.y;


        // define memory layout
        pixels.resizeErase(height, width);

        Imf::FrameBuffer frameBuffer;

        frameBuffer.insert("R",                                    // name
            Imf::Slice(Imf::FLOAT,                            // type
                (char*)&pixels[-dy][-dx].r,      // base
                sizeof(pixels[0][0]) * 1,     // xStride
                sizeof(pixels[0][0]) * width // yStride
            ));
        frameBuffer.insert("G",                                    // name
            Imf::Slice(Imf::FLOAT,                            // type
                (char*)&pixels[-dy][-dx].g,      // base
                sizeof(pixels[0][0]) * 1,     // xStride
                sizeof(pixels[0][0]) * width // yStride
            ));
        frameBuffer.insert("B",                                    // name
            Imf::Slice(Imf::FLOAT,                            // type
                (char*)&pixels[-dy][-dx].b,      // base
                sizeof(pixels[0][0]) * 1,     // xStride
                sizeof(pixels[0][0]) * width // yStride
            ));



        // read pixels
        file->setFrameBuffer(frameBuffer);
        file->readPixels(dw.min.y, dw.max.y);
    }
}




int main()
{
    std::cout << "Profile reading exr techniques" << "\n";
    std::cout << "==============================" << "\n\n";

    std::cout << "\n";
    std::cout << "With OIIO ImageBuffer" << "\n";
    std::cout << "---------------------" << "\n";
    

    OIIO::attribute("options", "threads=1,exr_threads=0,log_times=1");

    std::cout << "threads: " << OIIO::get_int_attribute("threads") << "\n";
    std::cout << "exr_threads: " << OIIO::get_int_attribute("exr_threads") << "\n";
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
    */

    {
        ImageCache* cache = ImageCache::create(false /* local cache */);
        cache->attribute("max_memory_MB", 16000.0f);
        cache->attribute("forcefloat ", 1);
        //cache->attribute("trust_file_extensions ", 1);
        //cache->attribute("autoscanline", 1);
        //cache->attribute("autotile", 64);
        profile::begin("read RGB from multichannel2");
        With_OIIO_ImageBuffer::read_RGB_from_multichannel2(cache);
        profile::end();

        profile::begin("- from cache");
        With_OIIO_ImageBuffer::read_RGB_from_multichannel2(cache);
        profile::end();
    }

    {
        ImageCache* cache = ImageCache::create(false /* local cache */);
        cache->attribute("max_memory_MB", 16000.0f);
        cache->attribute("forcefloat ", 1);
        //cache->attribute("trust_file_extensions ", 1);
        //cache->attribute("autoscanline", 1);
        //cache->attribute("autotile", 64);
        profile::begin("read RGB from multichannel3");
        With_OIIO_ImageBuffer::read_RGB_from_multichannel3(cache);
        profile::end();

        profile::begin("- from cache");
        With_OIIO_ImageBuffer::read_RGB_from_multichannel3(cache);
        profile::end();
    }

    {
        ImageCache* cache = ImageCache::create(false /* local cache */);
        cache->attribute("max_memory_MB", 16000.0f);
        cache->attribute("forcefloat ", 1);
        cache->attribute("trust_file_extensions ", 1);
        //cache->attribute("autoscanline", 1);
        cache->attribute("autotile", 64);
        profile::begin("read RGB from multichannel USE CACHE WHEN AVAILABLE");
        With_OIIO_ImageBuffer::read_RGB_from_multichannel_USE_CACHE_WHEN_AVAILABLE(cache);
        profile::end();

        profile::begin("- from cache");
        With_OIIO_ImageBuffer::read_RGB_from_multichannel_USE_CACHE_WHEN_AVAILABLE(cache);
        profile::end();
    }

    std::cout << "\n";
    std::cout << "With OIIO ImageCache" << "\n";
    std::cout << "---------------------" << "\n";
    {
        ImageCache* cache = ImageCache::create(false /* local cache */);
        cache->attribute("max_memory_MB", 16000.0f);
        //cache->attribute("forcefloat ", 1);
        //cache->attribute("autotile", 256);
        profile::begin("read RGB from multichannel");
        With_OIIO_ImageCache::read_RGB_from_multichannel(cache);
        profile::end();

        profile::begin("- from cache");
        With_OIIO_ImageCache::read_RGB_from_multichannel(cache);
        profile::end();
    }

    std::cout << "\n";
    std::cout << "With OIIO ImageInput" << "\n";
    std::cout << "--------------------" << "\n";
    profile::begin("read_ALL_from_multichannel");
    With_OIIO_ImageInput::read_ALL_from_multichannel();
    profile::end();

    profile::begin("read_RGB_from_multichannel");
    With_OIIO_ImageInput::read_RGB_from_multichannel();
    profile::end();

    profile::begin("read_RGB_from_multichannel to carray");
    With_OIIO_ImageInput::read_RGB_from_multichannel_carray();
    profile::end();

    std::cout << "\n";
    std::cout << "With OpenEXR" << "\n";
    std::cout << "------------" << "\n";
    Imf::setGlobalThreadCount(32);
    std::cout << "global thread count:" << Imf::globalThreadCount() << "\n";

    Imf::Array2D<float> rPixels;
    Imf::Array2D<float> gPixels;
    Imf::Array2D<float> bPixels;
    profile::begin("read RGB from multichannel");
    With_OpenEXR::read_RGB_from_multichannel(rPixels, gPixels, bPixels);
    profile::end();

    Imf::Array2D< With_OpenEXR::RGB> pixels;
    profile::begin("read RGB from multichannel interleaved");
    With_OpenEXR::read_RGB_from_multichannel_interleaved(pixels);
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
