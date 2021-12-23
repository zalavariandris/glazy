// read_exr.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;

#include "glazy.h"
#include "imdraw/imdraw.h"
#include "imdraw/imdraw_internal.h"
#include "widgets/Viewport.h"

#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/export.h>
using namespace OIIO;

#include <windows.h>

#include <functional>
#include <system_error>

#include <set>

#include "stringutils.h"
#include "pathutils.h"

#include <sstream>
#include <cstdio> // printf sprintf snprintf

#include <cassert>

void TextureViewer(GLuint* fbo, GLuint* color_attachment, Camera* camera, GLuint img_texture, const OIIO::ROI& roi) {
    auto item_pos = ImGui::GetCursorPos();
    auto item_size = ImGui::GetContentRegionAvail();
    static ImVec2 display_size;

    if (display_size.x != item_size.x || display_size.y != item_size.y) {
        //std::cout << "update fbo" << "\n";
        display_size = item_size;
        glDeleteTextures(1, color_attachment);
        *color_attachment = imdraw::make_texture_float(item_size.x, item_size.y, NULL);
        glDeleteFramebuffers(1, fbo);
        *fbo = imdraw::make_fbo(*color_attachment);
        camera->aspect = item_size.x / item_size.y;

    }

    ControlCamera(camera, item_size);
    ImGui::SetItemAllowOverlap();
    ImGui::SetCursorPos(item_pos);
    ImGui::Image((ImTextureID)*color_attachment, item_size, ImVec2(0, 1), ImVec2(1, 0));

    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glClearColor(0.2, 0.2, 0.2, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, item_size.x, item_size.y);
    imdraw::set_projection(camera->getProjection());
    imdraw::set_view(camera->getView());

    imdraw::quad(img_texture,
        { (float)roi.xbegin / 1000,  (float)roi.ybegin / 1000 },
        { -(float)roi.width() / 1000, -(float)roi.height() / 1000 }
    );
    imdraw::grid();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/*
  https://github.com/OpenImageIO/oiio/blob/e058dc8c4aa36ea42b4a19e26eeb56fa1fbc8973/src/iv/ivgl.cpp
  void typespec_to_opengl(const ImageSpec& spec, int nchannels, GLenum& gltype,
    GLenum& glformat, GLenum& glinternalformat);
*/

std::map<std::string, std::vector<int>> group_channels(const OIIO::ImageSpec& spec) {
    std::map<std::string, std::vector<int>> channel_groups;

    // main channels
    channel_groups["RGB_color"] = { 0,1,2 };
    channel_groups["Alpha"] = { 3 };
    channel_groups["ZDepth"] = { 4 };

    // AOVs
    for (auto i = 5; i < spec.nchannels; i++) {
        auto channel_name = spec.channelnames[i];

        auto channel_array = split_string(channel_name, ".");
        auto layer_name = channel_array.front();
        if (!channel_groups.contains(layer_name)) {
            channel_groups[layer_name] = std::vector<int>();
        }
        channel_groups[layer_name].push_back(i);
    }
    return channel_groups;
}

class State {
private:
    int _frame;
    std::filesystem::path _input_pattern;
    std::string _selected_layer;
    int _subimage;
    int _miplevel;

    mutable bool IMG_DIRTY{ true };
    mutable bool SPEC_DIRTY{ true };
    mutable bool ROI_DIRTY{ true };
    mutable bool LAYER_CHANNELS_DIRTY{ true };
    mutable bool TEXTURE_DIRTY{ true };
public:
    // State setters and getters
    void set_input_pattern(std::filesystem::path val) {
        _input_pattern = val;

        // dependents
        IMG_DIRTY = true;
    }
    std::filesystem::path input_pattern() const { return _input_pattern; }

    void set_selected_layer(std::string val) {
        static std::vector<bool*> dependants{ &TEXTURE_DIRTY, &ROI_DIRTY };
        _selected_layer = val;

        // notify dependants
        for (auto dep : dependants) { *dep = true; }
    }
    std::string selected_layer() const { return _selected_layer;}

    void set_frame(int val) {
        static std::vector<bool*> dependants{ &SPEC_DIRTY, &IMG_DIRTY };
        _frame = val;

        // notify dependants
        for (auto dep : dependants) { *dep = true; }
    }
    int frame() const{ return _frame;  }

    void set_subimage(int val) {
        static std::vector<bool*> dependants{ &SPEC_DIRTY, &TEXTURE_DIRTY };
        _subimage = val;

        // notify dependants
        for (auto dep : dependants) { *dep = true; }
    }
    int subimage() const{ return _subimage; }

    void set_miplevel(int val) {
        static std::vector<bool*> dependants{ &SPEC_DIRTY, &TEXTURE_DIRTY };
        _miplevel = val;

        // notify dependants
        for (auto dep : dependants) { *dep = true; }
    }
    int miplevel() const{ return _miplevel;  }


    // Computed

    OIIO::ImageBuf img() const {
        static OIIO::ImageBuf cache;
        static std::vector<bool*> dependants{ &ROI_DIRTY, &LAYER_CHANNELS_DIRTY, &SPEC_DIRTY };

        if (IMG_DIRTY) {
            // evaluate

            std::filesystem::path file = sequence_item(input_pattern(), frame());
            if (std::filesystem::exists(file)) {
                std::cout << "evaluate image" << "\n";
                cache = OIIO::ImageBuf(file.string().c_str());

                // notify dependants
                for (auto dep : dependants) { *dep = true; }
            }

            // clean flag
            IMG_DIRTY = false;
        }
        return cache;
    }

    OIIO::ImageSpec spec() const {
        static OIIO::ImageSpec cache;
        static std::vector<bool*> dependants{ &ROI_DIRTY, &LAYER_CHANNELS_DIRTY, &TEXTURE_DIRTY };

        if (SPEC_DIRTY) {
            // get global image cache
            auto is_image_cache = img().cachedpixels();
            ImageCache* image_cache = ImageCache::create(true /* global cache */);

            image_cache->get_imagespec(OIIO::ustring(img().name()), cache, subimage(), 0, false);

            // notify dependants
            for (auto dep : dependants) { *dep = true; }

            // clean flag
            SPEC_DIRTY = false;
        }

        return cache;
    }

    OIIO::ROI roi() const {
        static OIIO::ROI cache{0,1,0,1,0,1, 0, 3};
        static std::vector<bool*>dependants{&TEXTURE_DIRTY};

        if (ROI_DIRTY) {
            // evaluate
            std::cout << "evaluate roi" << "\n";
            std::map<std::string, std::vector<int>> layers = group_channels(spec());
            if (layers.contains(selected_layer())) {
                auto indices = layers[selected_layer()];
                cache = OIIO::ROI(0, spec().width, 0, spec().height, 0, 1, indices[0], indices[0] + indices.size());

                // notify dependants
                for (auto dep : dependants) { *dep = true; }
            }

            // clean flag
            ROI_DIRTY = false;
        }
        return cache;
    }

    std::map<std::string, std::vector<int>> layer_channels() {
        static std::map<std::string, std::vector<int>> cache;
        static std::vector<bool*> dependants{ &ROI_DIRTY };

        if (LAYER_CHANNELS_DIRTY) {
            // eval
            std::cout << "Evaluate layer channels" << "\n";
            cache = group_channels(spec());

            // notify dependants
            for (auto dep : dependants) { *dep = true; }
            
            //clean flag
            LAYER_CHANNELS_DIRTY = false;
        }
        return cache;
    }

    GLuint image_texture() {
        static GLuint cache = 0;
        if (TEXTURE_DIRTY) {
            /* EVALUATE */
            if (layer_channels().contains(selected_layer())) {
                std::cout << "\n";
                std::cout << "Evaluate textures" << "\n";
                std::cout << "- fetch pixels..." << "\n";

                std::vector<int> indices = layer_channels()[selected_layer()];
                //auto layer = OIIO::ImageBufAlgo::channels(img(), indices.size(), indices);
                static int data_size = roi().width() * roi().height() * roi().nchannels();
                static float* data = (float*)malloc(data_size * sizeof(float));
                if (data_size != roi().width() * roi().height() * roi().nchannels()) {
                    std::cout << "- reallocate pixels [" << roi().width() << "*" << roi().height() << "*" << roi().nchannels() << "]" << "\n";
                    free(data); // free allocated pixels data
                    data = (float*)malloc(data_size * sizeof(float));
                }

                // get global cache
                auto is_image_cache = img().cachedpixels();
                ImageCache* image_cache = ImageCache::create(true /* global cache */);

                //ImageSpec spec;
                //image_cache->get_imagespec(OIIO::ustring(img().name()), spec);
                int chbegin = roi().chbegin;
                int chend = roi().chend;
                auto success = image_cache->get_pixels(OIIO::ustring(img().name()), subimage(), miplevel(), 0, spec().width, 0, spec().height, 0, 1, chbegin, chend, TypeDesc::FLOAT, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, chbegin, chend);
                assert(("cant get pixels", success));

                //auto success = img().get_pixels(roi(), OIIO::TypeDesc::FLOAT, data);
                //auto success = layer.get_pixels(OIIO::ROI(), OIIO::TypeDesc::FLOAT, data);
                assert(success);

                /* make texture */
                std::cout << "- make texture..." << "\n";
                if (cache != 0) { glDeleteTextures(1, &cache); } // delete prev texture if exists
                cache = imdraw::make_texture_float(
                    roi().width(), roi().height(),
                    data, // data
                    roi().nchannels() == 3 ? GL_RGB : GL_RED, // internal format
                    roi().nchannels() == 3 ? GL_RGB : GL_RED, // format
                    GL_FLOAT, // type
                    GL_LINEAR, //min_filter
                    GL_NEAREST, //mag_filter
                    GL_REPEAT, //wrap_s
                    GL_REPEAT //wrap_t
                );
                if (roi().nchannels() == 1) {
                    glBindTexture(GL_TEXTURE_2D, cache);
                    GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
                    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }

                // notify dependents
                // no dependents
            }
            // clean flag
            TEXTURE_DIRTY = false;
        }
        return cache;
    }

};
State state;

/**
* convert OIIO typedesc to string
*/
std::string to_string(OIIO::TypeDesc type) {
    /* does not wor with switch for some reason. */
    if (type == OIIO::TypeHalf) return "half";
    if (type == OIIO::TypeFloat) return "float";
    if (type == OIIO::TypeUInt8) return "uint8";
    return "[Unknown TypeDesc type]";
}

int run_gui() {
    // start GUI
    glazy::init();

    // setup cache
    ImageCache* cache = ImageCache::create(true /* shared cache */);
    cache->attribute("max_memory_MB", 1024.0f * 16);
    cache->attribute("forcefloat", 1);
    //cache->attribute("autotile", 64);

    while (glazy::is_running()) {
        glazy::new_frame();

        ImGui::ShowStyleEditor();

        if (ImGui::Begin("Cache")) {
            auto cache = state.img().imagecache();
            if (cache) {
                int max_memory;
                cache->getattribute("max_memory_MB", TypeDesc::INT, &max_memory);
                ImGui::Text("%d", max_memory);
                if (ImGui::InputInt("set max memory MB", &max_memory)) {
                    cache->attribute("max_memory_MB", max_memory);
                }
                auto stats = cache->getstats();
                ImGui::Text("%s", stats.c_str());
            }
            ImGui::End();
        }

        if (ImGui::Begin("Read")) {
            static int START_FRAME = 10;
            static int END_FRAME = 20;
            //static fs::path sequence_filename;
            //static std::string derived_frame_filename;

            static fs::path sequence_pattern;

            if (ImGui::Button("Open...")) {
                auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0");
                //if (!filepath.empty()) {
                //    sequence_filename = filepath;

                //    find_sequence(sequence_filename, &START_FRAME, &END_FRAME);
                //    if (F < START_FRAME) F = START_FRAME;
                //    if (F > END_FRAME) F = END_FRAME;
                //}

                if (!filepath.empty()) {
                    auto [pattern, first, last] = scan_sequence(filepath);
                    state.set_input_pattern(pattern);
                    START_FRAME = first;
                    END_FRAME = last;
                    if (state.frame() < START_FRAME) state.set_frame(START_FRAME);
                    if (state.frame() > END_FRAME) state.set_frame(END_FRAME);
                }
            }

            static bool play;
            if (ImGui::Checkbox("play", &play)) {
                std::cout << "play changed:" << play << "\n";
            }

            if (play) {
                state.set_frame(state.frame() + 1);
                if (state.frame() > END_FRAME ) {
                    state.set_frame( START_FRAME ); // loop
                }
            }

            int F = state.frame();
            if (ImGui::SliderInt("frame", &F, START_FRAME, END_FRAME)) {
                state.set_frame(F);
            }

            static int subimage = state.subimage();
            if (ImGui::InputInt("subimage", &subimage)) {
                state.set_subimage(subimage);
            }


            ImGui::Text("sequence pattern: %s [%d-%d]", state.input_pattern().string().c_str(), START_FRAME, END_FRAME);
            ImGui::Text("image file: %s", state.img().name().c_str());


            if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("info")) {
                    auto spec = state.spec();
                    std::string image_format="initial format";
                    

                    ImGui::Text("%d x %d x %d, %d channel, %s", spec.width, spec.height, spec.depth, spec.nchannels, to_string(spec.format).c_str());



                    ImGui::Text("deep: %s", spec.deep ? "true" : "false");
                    ImGui::Text("nsubimage: %d", state.img().nsubimages());
                    ImGui::Text("subimage: %d", state.img().subimage());
                    ImGui::Text("nmiplevels: %d", state.img().nmiplevels());
                    ImGui::Text("miplevel: %d", state.img().miplevel());
                    
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("details")) {
                    auto info = state.spec().serialize(ImageSpec::SerialText);
                    ImGui::Text("%s", info.c_str());
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("layers")) {
                    if (ImGui::BeginTable("table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX))
                    {
                        auto layers = group_channels(state.spec());
                        for (const auto & [name, indices] : layers)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text(name.c_str());
                            ImGui::TableNextColumn();

                            for (auto i : indices) {
                                ImGui::TextColored({ 0.5,0.5,0.5,1.0 }, "#%i", i);
                                ImGui::SameLine();
                                ImGui::Text("%s", split_string(state.spec().channel_name(i), ".").back().c_str());
                                ImGui::SameLine();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::End();
        }

        if (ImGui::Begin("Viewer")) {
            auto layers = group_channels(state.spec());

            /******************
            * Show EXR layers *
            *******************/
            if (ImGui::BeginCombo("layers", state.selected_layer().c_str()))
            {
                for (const auto& [layer_name, indices] : state.layer_channels())
                {
                    bool is_selected = (layer_name == state.selected_layer());
                    if (ImGui::Selectable(layer_name.c_str(), is_selected)) {
                        state.set_selected_layer(layer_name);
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            
            /* display image */
            static GLuint fbo;
            static GLuint color_attachment;
            static Camera camera;
            TextureViewer(&fbo, &color_attachment, &camera, state.image_texture(), state.roi());
            ImGui::End();
        }

        /* show output filenames*/
        /*
        if (ImGui::Begin("Split Layers")) {
            if (!state.input_file().empty()) {
                if (ImGui::BeginTable("table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {

                    auto layers = group_channels(state.spec());
                    for (auto const & [layer_name, indices] : layers) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", layer_name.c_str());
                        ImGui::TableNextColumn();

                        auto layer_folder = std::filesystem::path(insert_layer_in_path(state.input_file(), layer_name)).parent_path();
                        auto layer_file = std::filesystem::path(insert_layer_in_path(state.input_file(), layer_name)).filename();
                        ImGui::Text("%s", layer_file.string().c_str());
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }
        */

        glazy::end_frame();
    }
    glazy::destroy();

    return EXIT_SUCCESS;
}

int main(int argc, char * argv[])
{
    return run_gui();
    return EXIT_SUCCESS;
}

