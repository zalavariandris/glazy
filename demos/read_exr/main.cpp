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


/*
  https://github.com/OpenImageIO/oiio/blob/e058dc8c4aa36ea42b4a19e26eeb56fa1fbc8973/src/iv/ivgl.cpp
  void typespec_to_opengl(const ImageSpec& spec, int nchannels, GLenum& gltype,
    GLenum& glformat, GLenum& glinternalformat);
*/

/**
  example:
  split_string("hello_world", "_")
  {"hello", "world"}
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

/*
  joint_string({"andris", "judit", "masa"}, ", ")
  "andris, judit, masa"
*/
inline std::string join_string(const std::vector<std::string> & tokens, const std::string &delimiter) {
    std::string text;
    // chan all but last items
    for (auto i = 0; i < tokens.size()-1; i++) {
        text += tokens[i];
        text += delimiter;
    }
    // append last item
    text += tokens.back();
    return text;
}


/*
  split digits from the end of the string
  split_digits("hello_4556")
  {"hello", "4556"}
*/
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

std::string to_string(OIIO::ImageBuf::IBStorage storage) {
    switch (storage)
    {
    case OpenImageIO_v2_3::ImageBuf::UNINITIALIZED: return "UNINITIALIZED";
    case OpenImageIO_v2_3::ImageBuf::LOCALBUFFER: return "LOCALBUFFER";
    case OpenImageIO_v2_3::ImageBuf::APPBUFFER: return "APPBUFFER";
    case OpenImageIO_v2_3::ImageBuf::IMAGECACHE: return "IMAGECACHE";
    default: return "[Inknown storage type]";
    }

}

inline std::string insert_layer_in_path(const fs::path & path,const std::string & layer_name) {
    auto [stem, digits] = split_digits(path.stem().string());
    auto layer_path = join_string({
        path.parent_path().string(),
        "/",
        stem,
        digits,
        digits.empty() ? "" : ".",
        layer_name,
        path.extension().string()
       }, "");
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

/*
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
        },"");

        // normalize path
        output_path = fs::path(output_path).lexically_normal().string();
        
        // write image
        std::cout << "writing image to:  " << output_path << std::endl;
        img.write(output_path);
    }
}
*/



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

    /* write each layer */
    for (auto const& [layer_name, indices] : channel_groups) {
        auto layer = OIIO::ImageBufAlgo::channels(img, indices.size(), indices);

        // insert layer into filename
        auto folder = input_file.parent_path();
        auto [stem, digits] = split_digits(input_file.stem().string());
        auto extension = input_file.extension();

        auto output_path = join_string({
            folder.string(),
            "/",
            stem,
            layer_name,
            digits.empty() ? "" : ".",
            digits,
            extension.string()
        }, "");

        // write image
        std::cout << "writing image to:  " << output_path << std::endl;
        layer.write(output_path);
    }

    /*
    auto layers = split_layers(img, channel_groups);

    std::cout << "Write files..." << std::endl;
    write_layers(input_file, layers);
    */
    std::cout << "Done!" << std::endl;
    return true;
}

std::vector<fs::path> find_sequence(fs::path input_path) {
    assert(std::filesystem::exists(input_path));

    std::vector<fs::path> sequence;
    // find sequence item in folder
    auto [input_name, input_digits] = split_digits(input_path.stem().string());
    auto folder = input_path.parent_path();

    for (std::filesystem::path path : fs::directory_iterator{ folder, fs::directory_options::skip_permission_denied }) {
        try {
            // match filename and digits count
            auto [name, digits] = split_digits(path.stem().string());
            bool IsSequenceItem = (name == input_name) && (digits.size()==input_digits.size());
            std::cout << "check file " << IsSequenceItem << " " << path << std::endl;
            //std::cout << name << " <> " << input_name << std::endl;
            if (IsSequenceItem) {
                sequence.push_back(path);
            }
        }
        catch (const std::system_error& ex) {
            std::cout << "Exception: " << ex.what() << "\n  probably std::filesystem does not support Unicode filenames" << std::endl;
        }
    }

    return sequence;
}

std::string to_string(std::vector<fs::path> sequence) {
    // collect frame numbers
    std::cout << "Sequence items:" << std::endl;
    std::vector<int> frame_numbers;
    for (auto seq_item : sequence) {
        auto [name, digits] = split_digits(seq_item.stem().string());
        frame_numbers.push_back(std::stoi(digits));
    }
    sort(frame_numbers.begin(), frame_numbers.end());

    // format sequence to human readable string
    int first_frame = frame_numbers[0];
    int last_frame = frame_numbers.back();

    auto [input_name, input_digits] = split_digits(sequence[0].stem().string());
    std::string text = input_name;
    for (auto i = 0; i < input_digits.size(); i++) {
        text += "#";
    }
    text += sequence[0].extension().string();
    text += " [" + std::to_string(first_frame) + "-" + std::to_string(last_frame) + "]";
    return text;
}


int run_cli(int argc, char* argv[]) {
    // setup cache
    ImageCache* cache = ImageCache::create(true /* shared cache */);
    cache->attribute("max_memory_MB", 1024.0f*16);
    cache->attribute("autotile", 64);
    //cache->attribute("forcefloat", 1);

    std::cout << "Parse arguments" << std::endl;
    // collect all paths with sequences
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

        // find sequence
        std::cout << "detect sequence for: " << path << std::endl;
        auto seq = find_sequence(path);

        // insert each item to paths
        for (auto item : seq) {
            paths.push_back(item);
        }
    }

    // keep uniq files only
    std::sort(paths.begin(), paths.end());
    paths.erase(unique(paths.begin(), paths.end()), paths.end());

    std::cout << "files found: " << std::endl;
    for (auto const& path : paths) {
        std::cout << "- " << path << std::endl;
    }
    
    
    /* process each file */
    std::cout << "Process files:" << std::endl;
    for (auto const & path : paths) {
       process_file(path);
    }
    /* exit */
    std::cout << "done." << "press any key to exit" << std::endl;
    
    getchar();
    return EXIT_SUCCESS;
}

void TextureViewer(GLuint* fbo, GLuint* color_attachment, Camera * camera, GLuint img_texture, const OIIO::ROI & roi) {
    auto item_pos = ImGui::GetCursorPos();
    auto item_size = ImGui::GetContentRegionAvail();
    static ImVec2 display_size;

    if (display_size.x != item_size.x || display_size.y != item_size.y) {
        //std::cout << "update fbo" << std::endl;
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
        { (float)roi.xbegin/1000,  (float)roi.ybegin  /1000 }, 
        { -(float)roi.width()/1000, -(float)roi.height()/1000}
    );
    imdraw::grid();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


class State {
private:
    int _frame;
    std::filesystem::path _input_file;
    std::string _selected_layer;

    mutable bool IMG_DIRTY{ true };
    mutable bool ROI_DIRTY{ true };
    mutable bool LAYER_CHANNELS_DIRTY{ true };
    mutable bool TEXTURE_DIRTY{ true };
public:
    void set_input_file(std::filesystem::path val) {
        _input_file = val;

        // dependents
        IMG_DIRTY = true;
    }
    std::filesystem::path input_file() const { return _input_file; }

    void set_selected_layer(std::string val) {
        static std::vector<bool*> dependants{ &TEXTURE_DIRTY, &ROI_DIRTY };
        _selected_layer = val;

        // notify dependants
        for (auto dep : dependants) { *dep = true; }
    }
    std::string selected_layer() const { return _selected_layer;}

    void set_frame(int val) {
        _frame = val;
    }
    int frame() {
        return _frame;
    }

    // process
    OIIO::ImageBuf img() const {
        static OIIO::ImageBuf cache;
        static std::vector<bool*> dependants{ &ROI_DIRTY, &LAYER_CHANNELS_DIRTY };

        if (IMG_DIRTY) {
            // evaluate
            if (std::filesystem::exists(_input_file)) {
                std::cout << "evaluate image" << std::endl;
                cache = OIIO::ImageBuf(_input_file.string().c_str());

                // notify dependants
                for (auto dep : dependants) { *dep = true; }
            }

            // clean flag
            IMG_DIRTY = false;
        }
        return cache;
    }

    OIIO::ROI roi() const {
        static OIIO::ROI cache{0,1,0,1,0,1, 0, 3};
        static std::vector<bool*>dependants{&TEXTURE_DIRTY};

        if (ROI_DIRTY) {
            // evaluate
            std::cout << "evaluate roi" << std::endl;
            std::map<std::string, std::vector<int>> layers = group_channels(img().spec());
            if (layers.contains(selected_layer())) {
                auto indices = layers[selected_layer()];
                cache = OIIO::ROI(0, img().spec().width, 0, img().spec().height, 0, 1, indices[0], indices[0] + indices.size());

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
            std::cout << "Evaluate layer channels" << std::endl;
            cache = group_channels(img().spec());

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
                std::cout << "Evaluate textures" << std::endl;
                std::cout << "- get pixels..." << std::endl;
                float* data = (float*)malloc(roi().width() * roi().height() * roi().nchannels() * sizeof(float));
                auto success = img().get_pixels(roi(), OIIO::TypeDesc::FLOAT, data);
                assert(success);

                /* make texture */
                std::cout << "- make texture..." << std::endl;
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
                free(data); // free allocated pixels data

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

std::string to_string(OIIO::TypeDesc type) {
    switch (type)
    {
        case OIIO::TypeDesc::UNKNOWN: return "UNKNOWN";
        case OIIO::TypeDesc::NONE: return "NONE";
        case OIIO::TypeDesc::UINT8: return "UINT8";
        case OIIO::TypeDesc::INT8: return "INT8";
        case OIIO::TypeDesc::UINT16: return "UINT16";
        case OIIO::TypeDesc::INT16: return "INT16";
        case OIIO::TypeDesc::UINT32: return "UINT32";
        case OIIO::TypeDesc::INT32: return "INT32";
        case OIIO::TypeDesc::UINT64: return "UINT64";
        case OIIO::TypeDesc::INT64: return "INT64";
        case OIIO::TypeDesc::HALF: return "HALF";
        case OIIO::TypeDesc::FLOAT: return "FLOAT";
        case OIIO::TypeDesc::DOUBLE: return "DOUBLE";
        case OIIO::TypeDesc::STRING: return "STRING";
        case OIIO::TypeDesc::PTR: return "PTR";
        case OIIO::TypeDesc::LASTBASE: return "LASTBASE";
        default: return "[Unknown TypeDesc type]";
    }
}

int run_gui() {
    // start GUI
    glazy::init();
    while (glazy::is_running()) {
        glazy::new_frame();

        ImGui::ShowStyleEditor();

        if (ImGui::Begin("Read")) {
            if (ImGui::Button("Open...")) {
                auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0");
                if (!filepath.empty()) {
                    state.set_input_file(filepath);
                }
            }
            ImGui::SameLine();
            ImGui::Text("%s", state.input_file().string().c_str());

            static int f = state.frame();
            if (ImGui::SliderInt("frame", &f, 0, 100)) {
                state.set_frame(f);
            }
            
            if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("info")) {
                    auto spec = state.img().spec();
                    ImGui::Text("%d x %d x %d, %d channel, %s", spec.width, spec.height, spec.depth, spec.nchannels, to_string(spec.format));

                    auto info = state.img().spec().serialize(ImageSpec::SerialText);
                    ImGui::Text("%s", info.c_str());
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("spec log")) {
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
            if (ImGui::BeginCombo("layers", state.selected_layer().c_str()))
            {
                for (const auto& [layer_name, indices] : state.layer_channels())
                {
                    const bool is_selected = layer_name == state.selected_layer();
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
    //const fs::path input_path{ "C:/Users/andris/Downloads/52_06_EXAM_v06-vrayraw.0002.exr" };
    auto img = OIIO::ImageBuf("C:/Users/zalav/Downloads/52_06/52_06_EXAM_v06-vrayraw.0013.exr");
    

    std::cout << "storage: " << to_string(img.storage()) << std::endl;
    std::cout << img.spec().serialize(ImageSpec::SerialText) << std::endl;
    //process_file(input_path);
    return EXIT_SUCCESS;
}

int main(int argc, char * argv[])
{

    //return test_process();

    if (argc > 1) {
        return run_cli(argc, argv);
    }
    else {
        return run_gui();
    }

    return EXIT_SUCCESS;
}

