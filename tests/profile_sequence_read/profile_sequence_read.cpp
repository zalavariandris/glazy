// profile_sequence_read.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "glazy.h"
#include "pathutils.h"
#include "ChannelsTable.h"
#include "widgets/ImageCacheWidget.h"
#include <chrono>
#include <assert.h>

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
#include <OpenEXR/ImfStdIO.h>
#include <OpenEXR/ImfMultiPartInputFile.h>



class Timer {
public:
    Timer(std::string name) : name(name) {
        start_time = std::chrono::steady_clock::now();
    }

    ~Timer() {
        auto end_time = std::chrono::steady_clock::now();
        auto dt = end_time - start_time;
        std::cout << name << ": " << std::chrono::duration_cast<std::chrono::milliseconds>(dt) << "\n";
    }
private:
    std::chrono::steady_clock::time_point start_time;
    std::string name;
};

#define PROFILE_SCOPE(name)  Timer oTimer##__LINE__(name);
#define PROFILE_FUNCTION()   PROFILE_SCOPE(__func__);

void WithImageInput(std::filesystem::path pattern, int first_frame, int last_frame)
{
    std::cout << "With ImageInput" << "\n";
    std::cout << "---------------" << "\n";
    for (int current_frame=first_frame;current_frame<last_frame; current_frame++)
    {
        auto begin = std::chrono::steady_clock::now();
        auto current_filename = sequence_item(pattern, current_frame);

        assert(("file does not exist", std::filesystem::exists(current_filename)));
        {
            auto inp = OIIO::ImageInput::open(current_filename.string());
            int nsubimages = 0;
            while (inp->seek_subimage(nsubimages, 0)) {
                nsubimages++;
            }
            inp->seek_subimage(0, 0);
        }

        auto end = std::chrono::steady_clock::now();
        auto dt = end - begin;
        std::cout << current_frame << " (" << std::chrono::duration_cast<std::chrono::milliseconds>(dt) << ")\n";

        current_frame++;
    }
}

void WithImageCache(std::filesystem::path pattern, int first_frame, int last_frame)
{
    auto image_cache = OIIO::ImageCache::create(true);
    image_cache->attribute("max_memory_MB", 1024.0f * 4);
    image_cache->attribute("max_open_files", 100);
    auto thread_info = image_cache->get_perthread_info();

    PROFILE_FUNCTION();
    for (int current_frame = first_frame; current_frame < last_frame; current_frame++)
    {
        PROFILE_SCOPE(std::to_string(current_frame));
        auto current_filename = sequence_item(pattern, current_frame);

        assert(("File does not exist!", std::filesystem::exists(current_filename)));
        {
            int nsubimages;
            static OIIO::ustring u_subimages("subimages");
            static OIIO::ustring u_channels("channels");
            OIIO::ustring uname(current_filename.string().c_str());

            auto handle = image_cache->get_image_handle(uname, thread_info);
            if (handle == NULL) std::cout << "handle is NULL!" << "\n";

            auto thread_info = image_cache->get_perthread_info();

            image_cache->get_image_info(handle, thread_info, 0, 0, u_subimages, OIIO::TypeInt, &nsubimages);

            for (auto p = 0; p < nsubimages; p++) {
                OIIO::ImageSpec spec;
                image_cache->get_imagespec(OIIO::ustring(current_filename.string()), spec, p, 0);
                for (auto c = 0; c < spec.nchannels; c++) {
                    auto channel_name = spec.channel_name(c);
                    std::cout << channel_name << ", ";
                }
                std::cout << "\n";
            }
        }
        current_frame++;
    }
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

void WithOpenEXR(std::filesystem::path pattern, int first_frame, int last_frame)
{
    PROFILE_FUNCTION();
    for (int current_frame = first_frame; current_frame < last_frame; current_frame++)
    {
        PROFILE_SCOPE(std::to_string(current_frame));
        auto current_filename = sequence_item(pattern, current_frame);

        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(current_filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, current_filename.string().c_str());
        auto file = new Imf::MultiPartInputFile(*inputStdStream);
        #else
        auto file = new Imf::MultiPartInputFile(filename.c_str());
        #endif

        //for (int p = 0; p < file->parts(); p++) {
        //    Imf::Header header = file->header(p);
        //    for (Imf::ChannelList::ConstIterator i = header.channels().begin();
        //        i != header.channels().end();
        //        ++i) {
        //        std::cout << i.name() << ", ";
        //    }
        //    std::cout << "\n";
        //}
        
    }
}


void WithGui(std::filesystem::path pattern, int first_frame, int last_frame)
{
    int current_frame = first_frame;
    auto image_cache = OIIO::ImageCache::create(true);
    image_cache->attribute("max_memory_MB", 1024.0f * 4);
    image_cache->attribute("max_open_files", 100);

    glazy::init();
    while (glazy::is_running())
    {
        glazy::new_frame();
        {
            // get current filename
            auto current_filename = sequence_item(pattern, current_frame);

            int nsubimages=0;
            static OIIO::ustring u_subimages("subimages");
            OIIO::ustring uname(current_filename.string().c_str());

            auto thread_info = image_cache->get_perthread_info();
            auto handle = image_cache->get_image_handle(uname, thread_info);
            if (handle == NULL) std::cout << "handle is NULL!" << "\n";

            image_cache->get_image_info(handle, thread_info, 0, 0, u_subimages, OIIO::TypeInt, &nsubimages);

            if (ImGui::Begin("imagecache")) {
                ImageCacheWidget(image_cache);
                ImGui::End();
            }

            if (ImGui::Begin("sequence")) {
                ImGui::Text("current filename: %s", current_filename.string().c_str());
                ImGui::Text("subimages: %d", nsubimages);
                ImGui::End();
            }
        }

        current_frame++;
        if (current_frame > last_frame) {
            current_frame = first_frame;
        }
        glazy::end_frame();
    }

    glazy::destroy();
}

int main()
{
    OIIO::attribute("threads", 0);
    OIIO::attribute("exr_threads", 1);
    OIIO::attribute("try_all_readers", 0);
    OIIO::attribute("openexr:core", 1);

    auto [file_pattern, first_frame, last_frame, current_frame] = scan_for_sequence("C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr");
    //std::filesystem::path file_pattern = "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.%04d.exr";
    //WithImageInput(file_pattern, first_frame, last_frame);
    WithImageCache(file_pattern, first_frame, last_frame);
    //WithGui(file_pattern, first_frame, last_frame);
    WithOpenEXR(file_pattern, first_frame, last_frame);
    return EXIT_SUCCESS;
}