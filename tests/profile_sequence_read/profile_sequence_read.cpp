// profile_sequence_read.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "glazy.h"
#include "pathutils.h"
#include "ChannelsTable.h"
#include "widgets/ImageCacheWidget.h"
#include <chrono>



OIIO::ImageCache *image_cache;


ChannelsTable my_get_channelstable(const std::filesystem::path& filename)
{
    if (!std::filesystem::exists(filename)) {
        return {};
    }


    int nsubimages;
    static OIIO::ustring u_subimages("subimages");
    OIIO::ustring uname(filename.string().c_str());
    image_cache->get_image_info(uname, 0, 0, u_subimages, OIIO::TypeInt, &nsubimages);

    //OIIO::ImageSpec spec;
    //image_cache->get_imagespec(OIIO::ustring(filename.string().c_str()), spec, 0, 0);
    //std::vector<std::string> views = get_stringvector_attribute(spec, "multiView");


    return {};
}

int main()
{
    image_cache = OIIO::ImageCache::create(true);
    image_cache->attribute("max_memory_MB", 1024.0f * 4);
    //image_cache->attribute("autotile", 64);
    image_cache->attribute("max_open_files", 100);

    auto thread_info = image_cache->get_perthread_info();

    std::filesystem::path file_pattern = "C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.%04d.exr";
    //std::filesystem::path file_pattern = "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.%04d.exr";
    int first_frame = 5;
    int last_frame = 56;
    int current_frame = first_frame;

    bool done_flag = false;

    //while (current_frame < last_frame)
    //{
    //    auto begin = std::chrono::steady_clock::now();
    //    auto current_filename = sequence_item(file_pattern, current_frame);

    //    int nsubimages;
    //    static OIIO::ustring u_subimages("subimages");
    //    OIIO::ustring uname(current_filename.string().c_str());
    //    
    //    auto handle = image_cache->get_image_handle(uname);
    //    if (handle == NULL) std::cout << "handle is NULL!" << "\n";
    //    image_cache->get_image_info(uname, 0, 0, u_subimages, OIIO::TypeInt, &nsubimages);

    //    auto end = std::chrono::steady_clock::now();
    //    auto dt = end - begin;
    //    std::cout << current_frame << " (" << std::chrono::duration_cast<std::chrono::milliseconds>(dt) << ")\n";
    //    //for (auto i = 0; i < std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() / 10; i++) {
    //    //    std::cout << ".";
    //    //}std::cout << "\n";
    //    current_frame++;

    //    if (!(current_frame < last_frame)) {
    //        current_frame = first_frame;
    //        std::cout << "loop" << "\n";
    //        image_cache->invalidate_all(true);
    //    }
    //}
    //return EXIT_SUCCESS;

    glazy::init();
    while (glazy::is_running())
    {
        glazy::new_frame();
        {
            // get current filename
            auto current_filename = sequence_item(file_pattern, current_frame);

            int nsubimages;
            static OIIO::ustring u_subimages("subimages");
            OIIO::ustring uname(current_filename.string().c_str());

            auto handle = image_cache->get_image_handle(uname);
            if (handle == NULL) std::cout << "handle is NULL!" << "\n";
            image_cache->get_image_info(uname, 0, 0, u_subimages, OIIO::TypeInt, &nsubimages);
            
            if (ImGui::Begin("imagecache")) {
                ImageCacheWidget(image_cache);
                ImGui::End();
            }

            if (ImGui::Begin("sequence")) {
                ImGui::Text("current filename: %s", current_filename.string().c_str());
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

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
