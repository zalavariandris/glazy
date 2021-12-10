// read_exr.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include "glazy.h"
#include "imdraw/imdraw.h"
#include "imdraw/imdraw_internal.h"
#include "widgets/Viewport.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/export.h>

#include <windows.h>


/*
  https://github.com/OpenImageIO/oiio/blob/e058dc8c4aa36ea42b4a19e26eeb56fa1fbc8973/src/iv/ivgl.cpp
  void typespec_to_opengl(const ImageSpec& spec, int nchannels, GLenum& gltype,
    GLenum& glformat, GLenum& glinternalformat);
*/

inline std::vector<std::string> split_string(std::string text, const std::string &delimiter) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    while ((pos = text.find(delimiter)) != std::string::npos) {
        auto token = text.substr(0, pos);
        tokens.push_back(token);
        text.erase(0, pos + delimiter.length());
    }
    tokens.push_back(text);
    return tokens;
};

inline std::string join_string(const std::vector<std::string> & tokens) {
    std::string text;
    for (auto token : tokens) {
        text += token;
    }
    return text;
}

inline std::tuple<std::string, std::string> split_digits(const std::string & stem) {
    assert(!stem.empty());
    int digits_count = 0;
    while (std::isdigit(stem[stem.size() - 1 - digits_count])) {
        digits_count++;
    }

    auto text = stem.substr(0, stem.size() - digits_count);
    auto digits = stem.substr(stem.size() - digits_count, -1);

    return { text, digits };
}

inline std::string insert_layer_in_path(const std::filesystem::path & path,const std::string & layer_name) {
    auto [stem, digits] = split_digits(path.stem().string());
    auto layer_path = join_string({
        path.parent_path().string(),
        "/",
        stem,
        digits,
        digits.empty() ? "" : ".",
        layer_name,
        path.extension().string()
       });
    return layer_path;
}

std::map<std::string, std::vector<int>> group_channels(const OIIO::ImageSpec & spec) {
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

std::map<std::string, OIIO::ImageBuf> split_layers(const OIIO::ImageBuf & img, const std::map<std::string, std::vector<int>> & channel_groups) {
    std::cout << "Split layers..." << std::endl;
    std::map<std::string, OIIO::ImageBuf> layers;
    for (auto const & [layer_name, indices] : channel_groups) {
        std::cout << "  " << layer_name << std::endl;
        layers[layer_name] = OIIO::ImageBufAlgo::channels(img, indices.size(), indices);
    }

    return layers;
}



void write_layers(const std::filesystem::path & output_path, const std::map<std::string, OIIO::ImageBuf> & layers) {
    for (auto const & [layer_name, img] : layers) {
        // insert layer into filename
        auto folder = output_path.parent_path();
        auto [stem, digits] = split_digits(output_path.stem().string());
        auto extension = output_path.extension();

        auto output_path = join_string({
            folder.string(),
            "/",
            stem,
            layer_name,
            digits.empty() ? "" : ".",
            digits,
            extension.string()
        });
        std::cout << "  " << output_path << std::endl;

        // write image
        img.write(output_path);
    }
}

using namespace OIIO;

bool process_file(const std::filesystem::path & input_file) {
    std::cout << "Read image: " << input_file << "..." << std::endl;
    auto img = OIIO::ImageBuf(input_file.string().c_str());
    auto spec = img.spec();

    std::cout << "List all channels: " << std::endl;
    for (auto i = 0; i < spec.nchannels; i++) {
        std::cout << "- " << "#" << i << " " << spec.channel_name(i) << std::endl;
    }

    std::cout << "Group channels into layers..." << std::endl;
    auto channel_groups = group_channels(img.spec());
    for (auto const & [name, indices] : channel_groups) { // print channels
        std::cout << "  " << name << ": ";
        std::vector<std::string> names;
        for (auto i : indices) {
            std::cout << "#" << i << split_string(spec.channel_name(i), ".").back() << ", ";
        }
        std::cout << std::endl;
    }

    auto layers = split_layers(img, channel_groups);

    std::cout << "Write files..." << std::endl;
    write_layers(input_file, layers);

    std::cout << "Done!" << std::endl;
    return true;
}

int run_cli(int argc, char* argv[]) {
    std::cout << "Parse arguments" << std::endl;

    std::vector<std::filesystem::path> paths;
    for (auto i = 1; i < argc; i++) {
        std::filesystem::path path{ argv[i] };
        if (!std::filesystem::exists(path)) {
            std::cout << "- " << "file does not exists: " << path;
            continue;
        }

        if (!std::filesystem::is_regular_file(path)) {
            std::cout << "- " << "not a file: " << path;
        }

        if (path.extension() != ".exr") {
            std::cout << "- " << "not exr" << path;
            continue;
        }

        paths.push_back(path);
    }

    std::cout << "Process files:" << std::endl;
    for (auto const & path : paths) {
        process_file(path);
    }

    getchar();
    return EXIT_SUCCESS;
}

void TextureViewer(GLuint* fbo, GLuint* color_attachment, Camera * camera, GLuint img_texture, const OIIO::ROI & roi) {
    auto item_pos = ImGui::GetCursorPos();
    auto item_size = ImGui::GetContentRegionAvail();
    static ImVec2 display_size;

    if (display_size.x != item_size.x || display_size.y != item_size.y) {
        display_size = item_size;
        glDeleteTextures(1, color_attachment);
        *color_attachment = imdraw::make_texture_float(item_size.x, item_size.y, NULL);
        glDeleteFramebuffers(1, fbo);
        *fbo = imdraw::make_fbo(*color_attachment);
        camera->aspect = item_size.x / item_size.y;
        std::cout << "update fbo" << std::endl;
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
        { (float)roi.xbegin/1000,  (float)roi.ybegin  /1000 }, 
        { -(float)roi.width()/1000, -(float)roi.height()/1000}
    );
    imdraw::grid();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

class State {
private:
    std::filesystem::path _input_file;
    OIIO::ImageBuf _img;
    OIIO::ROI _roi{ 0, 1, 0, 1 };


public:
    std::filesystem::path input_file() const{
        return _input_file;
    }

    void set_input_file(std::filesystem::path val) {
        _input_file = val;
        _img = OIIO::ImageBuf(_input_file.string().c_str());
    }

    OIIO::ImageBuf img() const {
        return _img;
    }
    
    OIIO::ROI roi() const {
        return _roi;
    }

    void set_roi(OIIO::ROI val) {
        _roi = val;
    }
};
State state;

int run_gui() {
    // start GUI
    glazy::init();
    while (glazy::is_running()) {
        glazy::new_frame();

        ImGui::ShowStyleEditor();

        if (ImGui::Begin("Read")) {
            ImGui::Text("%s", state.input_file().string().c_str());
            ImGui::SameLine();
            if (ImGui::Button("Open...")) {
                auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0");
                if (!filepath.empty()) {
                    state.set_input_file(filepath);
                }
            }
            
            if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("info")) {
                    auto info = state.img().spec().serialize(ImageSpec::SerialText);
                    ImGui::Text("%s", info.c_str());
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("layers")) {
                    if (ImGui::BeginTable("table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX))
                    {
                        auto layers = group_channels(state.img().spec());
                        for (const auto & [name, indices] : layers)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text(name.c_str());
                            ImGui::TableNextColumn();

                            for (auto i : indices) {
                                ImGui::TextColored({ 0.5,0.5,0.5,1.0 }, "#%i", i);
                                ImGui::SameLine();
                                ImGui::Text("%s", split_string(state.img().spec().channel_name(i), ".").back().c_str());
                                ImGui::SameLine();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();

                if (ImGui::Button("Save")) {
                    std::cout << "save" << std::endl;
                }
            }

            ImGui::End();
        }

        if (ImGui::Begin("Viewer")) {
            std::vector<std::string> items;
            auto layers = group_channels(state.img().spec());

            /******************
            * Show EXR layers *
            *******************/
            static std::string selected_layer = "";
            if (ImGui::BeginCombo("layers", selected_layer.c_str()))
            {
                for (const auto& [layer_name, indices] : layers)
                {
                    const bool is_selected = layer_name == selected_layer;
                    if (ImGui::Selectable(layer_name.c_str(), is_selected))
                        selected_layer = layer_name;

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            /***************
            * Update image *
            ****************/
            static GLuint img_texture;
            static std::string display_layer;

            std::cout << state.roi().width() << std::endl;
            
            if (display_layer != selected_layer) {
                display_layer = selected_layer;
                std::cout << "update image texture..." << std::endl;
                auto indices = layers[display_layer];
                std::cout << "get layer img...";
                for (auto idx : indices) {
                    std::cout << idx << ",";
                }; std::cout << std::endl;

                // update zoom ROI
                auto roi = OIIO::ROI(0, state.img().spec().width, 0, state.img().spec().height, 0, 1, indices[0], indices[0] + indices.size());
                state.set_roi(roi);
                std::cout << "get pixels..." << std::endl;
                float* data = (float*)malloc(state.roi().width() * state.roi().height() * state.roi().nchannels() * sizeof(float));
                auto success = state.img().get_pixels(state.roi(), OIIO::TypeDesc::FLOAT, data);
                assert(success);

                /* recreate texture */
                glDeleteTextures(1, &img_texture);
                img_texture = imdraw::make_texture_float(
                    state.roi().width(), state.roi().height(),
                    data, // data
                    state.roi().nchannels() == 3 ? GL_RGB : GL_RED, // internal format
                    state.roi().nchannels() == 3 ? GL_RGB : GL_RED, // format
                    GL_FLOAT, // type
                    GL_LINEAR, //min_filter
                    GL_NEAREST, //mag_filter
                    GL_REPEAT, //wrap_s
                    GL_REPEAT //wrap_t
                );
                if (state.roi().nchannels() == 1) {
                    glBindTexture(GL_TEXTURE_2D, img_texture);
                    GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
                    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                free(data);
                std::cout << "done" << std::endl;
            }

            /* display image */
            static GLuint fbo;
            static GLuint color_attachment;
            static Camera camera;
            TextureViewer(&fbo, &color_attachment, &camera, img_texture, state.roi());
            ImGui::End();
        }

        /* show output filenames*/
        if (ImGui::Begin("Split Layers")) {
            if (!state.input_file().empty()) {
                if (ImGui::BeginTable("table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {

                    auto layers = group_channels(state.img().spec());
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

        glazy::end_frame();
    }
    glazy::destroy();

    return EXIT_SUCCESS;
}

int test_process() {
    const std::filesystem::path input_path{ "C:/Users/andris/Downloads/52_06_EXAM_v06-vrayraw.0002.exr" };
    process_file(input_path);
    return 0;
}

int main(int argc, char * argv[])
{

    if (argc > 1) {
        return run_cli(argc, argv);
    }
    else {
        return run_gui();
    }

    getchar();
    return 0;
}

