// Nodes.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include <vector>
#include <functional>
#include <any>


// ImGui
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h" // use imgui math operators

#include "glazy.h"

#include <filesystem>
#include <tuple>

#include "OpenImageIO/imageio.h"

#include "glad/glad.h"

#include "../../tracy/Tracy.hpp"
#include "Camera.h"

#include <format>

#include "ImGuiWidgets.h"

#include "nodes.h"

#include <future>

#include "pathutils.h"

#include <opencv2/opencv.hpp>

class RenderTexture
{
private:
    int mWidth, mHeight;
    GLuint colorattachment;
    GLuint fbo;
public:
    RenderTexture(int w, int h):mWidth(w), mHeight(h) {
        colorattachment = imdraw::make_texture(w, h, NULL, GL_RGBA);
        fbo = imdraw::make_fbo(colorattachment);
    }

    void resize(int w, int h)
    {

        if (w == mWidth && h == mHeight) return;

        if (glIsTexture(colorattachment)) {
            glDeleteTextures(1, &colorattachment);
        }

        colorattachment = imdraw::make_texture(w, h, NULL, GL_RGBA);
        fbo = imdraw::make_fbo(colorattachment);

        mWidth = w;
        mHeight = h;
    }

    void begin() const
    {
        // setup fbo
        ImVec4 viewport(0.0f, 0.0f, width(), height()); // x, y, w, h
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(viewport.x, viewport.y, viewport.z, viewport.w);

        // clear canvas    
        glClearColor(0, 0, 0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // draw to fbo
        // ...
    }

    void end() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GLuint color() {
        return colorattachment;
    }

    int width() const
    {
        return mWidth;
    }

    int height() const
    {
        return mHeight;
    }

    std::tuple<int,int> size() {
        return { mWidth, mHeight };
    }

    void operator()(std::function<void()> cb)
    {
        begin();
        cb();
        end();
    }

    void operator()(int w, int h, std::function<void()> cb)
    {
        resize(w, h);
        begin();
        cb();
        end();
    }
};

#include "MovieIO.h"

class ReadNode
{
private:
    int _F;
    std::string _cache_pattern;

    using MemoryImage = std::tuple<void*, int,int,int>; //ptr, width, height, channels

    int _first_frame;
    int _last_frame;
    bool _is_movie;

    std::filesystem::path _current_filepath;
    std::unique_ptr<MovieIO::MovieInput> _current_file;


public:
    Nodes::Attribute<std::string> file; // filename or sequence pattern
    Nodes::Attribute<int> frame;
    Nodes::Outlet<std::tuple<void*, std::tuple<int, int, int,int, int>>> plate_out;

    void* memory;
    size_t capacity{ 0 };

    std::map<int, MemoryImage> _cache; // memory, width, height, channels

    int first_frame() const{
        return _first_frame;
    }

    int last_frame() const{
        return _last_frame;
    }

    int length() const {
        return _last_frame - _first_frame + 1;
    }

    void open(std::string filepath) {
        _cache.clear(); // clear the cache

        bool Exist = std::filesystem::exists(filepath);

        std::cout << "ReadNode->open: " << filepath << "\n";

        _current_file = MovieIO::MovieInput::open(filepath);
        auto [b, e] = _current_file->range();
        _first_frame = b;
        _last_frame = e;
        file.set(filepath);
        frame.set(_first_frame);
    }

    ReadNode()
    {
        // alloc default memory with a small imagesize
        capacity = 256 * 256 * 4 * sizeof(half);
        memory = std::malloc(capacity);
        
        // setup inlet triggers
        file.onChange([&](std::string pattern) {
            frame.set(_first_frame);
        });

        frame.onChange([&](int F){
            _F = frame.get();
            read();
        });
    }

    ~ReadNode()
    {
        free(memory);
    }

    void read()
    {
        if (!_cache.contains(_F))
        {
            ZoneScoped;

            // reopen the file if filename changed
            if (_current_filepath != file.get()) {
                _current_filepath = file.get();
                _current_file = MovieIO::MovieInput::open(file.get());
                std::cout << "open file" << "\n";
            }

            _current_file->seek(frame.get());
            auto info = _current_file->info();
            int nchannels = 4;
            auto required_capacity = info.width * info.height * nchannels * sizeof(half);
            if (capacity < required_capacity)
            {
                std::cout << "reader->realloc" << "\n";
                if (void* new_memory = std::realloc(memory, required_capacity))
                {
                    memory = new_memory;
                    capacity = required_capacity;
                }
                else {
                    throw std::bad_alloc();
                }
            }

            _current_file->read(memory);
            auto& img_cache = _cache[_F];
            std::get<0>(img_cache) = malloc(capacity);
            std::memcpy(std::get<0>(img_cache), memory, capacity);
            std::get<1>(img_cache) = info.width;
            std::get<2>(img_cache) = info.height;
            std::get<3>(img_cache) = nchannels;
        }

        auto [memory, w, h, c] = _cache.at(_F);
        plate_out.trigger({ memory,{0,0,w,h,c} });
    }

    std::vector<std::tuple<int, int>> cached_range() const{
        // collect cache frame ranges
        std::vector<int> cached_frames;
        for (const auto& [F, img] : _cache)
        {
            cached_frames.push_back(F);
        }
        std::sort(cached_frames.begin(), cached_frames.end());

        std::vector<std::tuple<int, int>> ranges;

        for (auto F : cached_frames) {
            if (!ranges.empty()) {
                auto& last_range = ranges.back();
                auto [begin, end] = last_range;
                if (F == end + 1) {
                    std::get<1>(last_range)++;
                }
                else {
                    ranges.push_back({ F, F });
                }
            }
            else {
                ranges.push_back({ F, F });
            }
        }

        return ranges;
    }

    void onGUI() {

        std::string filename = file.get();
        if (ImGui::InputText("file", &filename)) {
            file.set(filename);
        }
        ImGui::SameLine();

        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::GetTextLineHeightWithSpacing());
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeightWithSpacing());
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##open")) {
            auto selected_file = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg\0MP4 movies\0*.mp4");
            if (!selected_file.empty()) open(selected_file.string());
        }
        ImGui::LabelText("range", "%d-%d", _first_frame, _last_frame);
        ImGui::SliderInt("frame", &frame, _first_frame, _last_frame);

        static bool is_playing{ false };
        ImGui::Checkbox("play", &is_playing);
        if (is_playing) {
            int F = frame.get();
            F++;
            if (F > _last_frame) {
                F = _first_frame;
            }
            frame.set(F);
        }

        //ImGui::LabelText("filename", "%s", _filename.filename().string().c_str());

        int used_memory=0;
        for (const auto& [key, memory_img] : _cache) {
            const auto& [mem, w,h,c] = memory_img;
            used_memory += w * h * c * sizeof(half);
        }
        ImGui::LabelText("images", "%d", _cache.size());
        ImGui::LabelText("memory", "%.2f MB", used_memory / pow(1000, 2) );

        for (const auto& inlet : plate_out.targets()) {
            ImGui::Text("inlet: %s", "target");
        }

        ImGui::Ranges(cached_range(), _first_frame, _last_frame);
    }
};


class ViewportNode {
private:
    
    GLuint _fbo;
    GLuint _program;

    GLint glinternalformat = GL_RGBA16F;

    enum class DeviceTransform : int{
        Linear=0, sRGB=1, Rec709=2
    };

    ImVec2 pan{ 0,0 };
    float zoom{ 1 };

public:
    Nodes::Inlet<std::tuple<void*, std::tuple<int, int, int, int, int>>> image_in{ "image_in" };
    Nodes::Attribute<float> gamma{ 1.0 };
    Nodes::Attribute<float> gain{0.0};
    Nodes::Attribute<DeviceTransform> selected_device{DeviceTransform::sRGB};

    GLuint _datatex = -1;
    GLuint _correctedtex = -1;
    int _width = 0;
    int _height = 0;
    int _nchannels = 0;

    ViewportNode()
    {
        init_shader();
        image_in.onTrigger([&](auto plate) {
            
            ZoneScopedN("upload to texture");
            auto [ptr, bbox] = plate;
            auto [x, y, w, h, c] = bbox;

            // update texture size and bounding box
            if (_width != w || _height != h || _nchannels!=c) {
                _width = w;
                _height = h; 
                _nchannels = c;
                init_texture();
            }

            update_texture(ptr, w, h, c);
            color_correct_texture();
        });

        gamma.onChange([&](auto val) {color_correct_texture(); });
        gain.onChange([&](auto val) {color_correct_texture(); });
    }

    void init_texture()
    {
        std::cout << "Viewport->init texture" << "\n";

        if (glIsTexture(_datatex)) glDeleteTextures(1, &_datatex);
        glGenTextures(1, &_datatex);
        glBindTexture(GL_TEXTURE_2D, _datatex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, _width, _height);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (glIsTexture(_correctedtex)) glDeleteTextures(1, &_correctedtex);
        glGenTextures(1, &_correctedtex);
        glBindTexture(GL_TEXTURE_2D, _correctedtex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, _width, _height);
        glBindTexture(GL_TEXTURE_2D, 0);

        // init fbo
        if (glIsFramebuffer(_fbo)) {
            glDeleteFramebuffers(1, &_fbo);
        }
        glGenFramebuffers(1, &_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _correctedtex, 0);


        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

    }

    void update_texture(void* ptr, int w, int h, int c) {
        // update texture
        GLenum glformat;
        switch (c)
        {
        case 3:
            glformat = GL_RGB;
            break;
        case 4:
            glformat = GL_RGBA;
            break;
        default:
            glformat = GL_RGB;
            break;
        }

        // transfer pixels to texture
        //auto [x, y, w, h] = m_bbox;
        glBindTexture(GL_TEXTURE_2D, _datatex);
        //glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, GL_HALF_FLOAT, ptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void init_shader() {
        const char* PASS_THROUGH_VERTEX_CODE = R"(
                #version 330 core
                layout (location = 0) in vec3 aPos;
                void main()
                {
                    gl_Position = vec4(aPos, 1.0);
                }
                )";

        const char* display_correction_fragment_code = R"(
        #version 330 core
        out vec4 FragColor;
        uniform mediump sampler2D inputTexture;
        uniform vec2 resolution;

        uniform float gamma_correction;
        uniform float gain_correction;
        uniform int convert_to_device; // 0:linear | 1:sRGB | 2:Rec709

        float sRGB_to_linear(float channel){
            return channel <= 0.04045
                ? channel / 12.92
                : pow((channel + 0.055) / 1.055, 2.4);
        }

        vec3 sRGB_to_linear(vec3 color){
            return vec3(
                sRGB_to_linear(color.r),
                sRGB_to_linear(color.g),
                sRGB_to_linear(color.b)
                );
        }

        float linear_to_sRGB(float channel){
                return channel <= 0.0031308f
                ? channel * 12.92
                : pow(channel, 1.0f/2.4) * 1.055f - 0.055f;
        }

        vec3 linear_to_sRGB(vec3 color){
            return vec3(
                linear_to_sRGB(color.r),
                linear_to_sRGB(color.g),
                linear_to_sRGB(color.b)
            );
        }

        const float REC709_ALPHA = 0.099f;
        float linear_to_rec709(float channel) {
            if(channel <= 0.018f)
                return 4.5f * channel;
            else
                return (1.0 + REC709_ALPHA) * pow(channel, 0.45f) - REC709_ALPHA;
        }

        vec3 linear_to_rec709(vec3 rgb) {
            return vec3(
                linear_to_rec709(rgb.r),
                linear_to_rec709(rgb.g),
                linear_to_rec709(rgb.b)
            );
        }


        vec3 reinhart_tonemap(vec3 hdrColor){
            return vec3(1.0) - exp(-hdrColor);
        }

        void main(){
            vec2 uv = gl_FragCoord.xy/resolution;
            vec3 rawColor = texture(inputTexture, uv).rgb;
                
            // apply corrections
            vec3 color = rawColor;

            // apply exposure correction
            color = color * pow(2, gain_correction);

            // exposure tone mapping
            //mapped = reinhart_tonemap(mapped);

            // apply gamma correction
            color = pow(color, vec3(gamma_correction));

            // convert color to device
            if(convert_to_device==0) // linear, no conversion
            {

            }
            else if(convert_to_device==1) // sRGB
            {
                color = linear_to_sRGB(color);
            }
            if(convert_to_device==2) // Rec.709
            {
                color = linear_to_rec709(color);
            }

            FragColor = vec4(color, texture(inputTexture, uv).a);
        }
        )";

        if (glIsProgram(_program)) {
            glDeleteProgram(_program);
        }
        _program = imdraw::make_program_from_source(PASS_THROUGH_VERTEX_CODE, display_correction_fragment_code);
        if (glIsFramebuffer(_fbo)) {
            glDeleteFramebuffers(1, &_fbo);
        }
        _fbo = imdraw::make_fbo(_correctedtex);
    }

    void color_correct_texture()
    {
        // begin render to tewxture
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glViewport(0, 0, _width, _height);

        // draw to fbo
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        //glEnable(GL_BLEND);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        imdraw::push_program(_program);
        /// Draw quad with fragment shader
        imgeo::quad();
        static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
        static auto vao = imdraw::make_vao(_program, { {"aPos", {vbo, 3}} });

        imdraw::set_uniforms(_program, {
            {"inputTexture", 0},
            {"resolution", glm::vec2(_width, _height)},
            {"gain_correction", gain.get()},
            {"gamma_correction", 1.0f / gamma.get()},
            {"convert_to_device", (int)selected_device.get()},
            });
        glBindTexture(GL_TEXTURE_2D, _datatex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        imdraw::pop_program();

        // end render to texture
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }


    void onGUI() {

        ImVec2 itemsize;
        ImGui::BeginGroup(); // display correction
        {
            //static const std::vector<std::string> devices{ "linear", "sRGB", "Rec.709" };
            //int devices_combo_width = ImGui::CalcComboWidth(devices);
            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 6);
            ImGui::SliderFloat(ICON_FA_ADJUST "##gain", &gain, -6.0f, 6.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("gain");
            if (ImGui::IsItemClicked(1)) gain.set(0.0);

            ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 6);
            ImGui::SliderFloat("##gamma", &gamma, 0, 4);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("gamma");
            if (ImGui::IsItemClicked(1)) gamma.set(1.0);


            //ImGui::SetNextItemWidth(devices_combo_width);
            //ImGui::Combo("##device", &selected_device, "linear\0sRGB\0Rec.709\0");
            //if (ImGui::IsItemHovered()) ImGui::SetTooltip("device");

            ImGui::ImageViewer("viewer1", (ImTextureID)_correctedtex, _width, _height, &pan, &zoom);
            itemsize = ImGui::GetItemRectSize();
        }
        ImGui::EndGroup();
    }
};


int main(int argc, char* argv[])
{
    /// Open with file argument
    // Fix Current Working Directory
    if (argc > 0) {
        try {

            std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path()); // set working directory to exe
        }
        catch (std::filesystem::filesystem_error const& ex) {
            std::cout
                << "what():  " << ex.what() << '\n'
                << "path1(): " << ex.path1() << '\n'
                << "path2(): " << ex.path2() << '\n'
                << "code().value():    " << ex.code().value() << '\n'
                << "code().message():  " << ex.code().message() << '\n'
                << "code().category(): " << ex.code().category().name() << '\n';
        }
    }
    std::cout << "current working directory: " << std::filesystem::current_path() << "\n";

    glazy::init({.docking=true, .viewports=false});


    auto& style = ImGui::GetStyle();
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;
    style.PopupBorderSize = 0;
    style.FrameBorderSize = 0;
    glazy::set_vsync(false);

    // create state
    bool IS_PLAYING = false;

    // create graph
    auto read_node = ReadNode();
    auto viewport_node = ViewportNode();
    read_node.plate_out.target(viewport_node.image_in);

    // Open file
    if (argc == 2)
    {
        // open dropped file
        read_node.open(argv[1]);
    }
    else
    {
        // open default sequence
        //read_node.open("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0004.exr");
        //read_node.open("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.%04d.exr");
        //read_node.open("C:/Users/andris/Desktop/testimages/58_31_FIRE_v20.mp4");
    }
    
    //viewport_node.fit();

    while (glazy::is_running()) {
        glazy::new_frame();
        FrameMark;

        ImVec2 itemsize;
        static ImVec2 pan{ 0,0 };
        static float zoom{ 1 };

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open", "")) {
                    //filesequence_node.open();
                    //read_node.filename.set("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("100%", "")) {
                    zoom = 1.0;
                }

                if (ImGui::MenuItem("Center", "")) {
                    pan = ImVec2(0, 0);
                }

                if (ImGui::MenuItem("Fit", "")) {
                    zoom = std::min(itemsize.x / viewport_node._width, itemsize.y / viewport_node._height);
                    pan = ImVec2(0, 0);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (ImGui::Begin("Viewer##Panel")) {

            ImGui::ImageViewer("viewer", (ImTextureID)viewport_node._correctedtex, viewport_node._width, viewport_node._height, &pan, &zoom, { -1,-1 });
            itemsize = ImGui::GetItemRectSize();
        }
        ImGui::End();

        //auto viewsize = ImGui::GetMainViewport()->WorkSize;
        
        if (read_node.length() > 0) {
            //ImGui::SetNextWindowSize({ viewsize.x * 2 / 3,0 });
            //ImGui::SetNextWindowPos({ viewsize.x * 1 / 3 / 2, viewsize.y - 80 });
            if (ImGui::Begin("Frameslider##Panel"))
            {
                int F = read_node.frame.get();
                // filesequence_node.play.get();

                //if (ImGui::SliderInt("frameslider", &F, read_node.first_frame(), read_node.last_frame())) {
                //    read_node.frame.set(F);
                //}

                if (ImGui::Frameslider("frameslider", &IS_PLAYING, &F, read_node.first_frame(), read_node.last_frame(), read_node.cached_range()))
                {
                    read_node.frame.set(F);
                }
            }
            ImGui::End();
        }

        //ImGui::SetNextWindowSize({ 300, viewsize.y * 2 / 3 });
        //ImGui::SetNextWindowPos({ viewsize.x-300, viewsize.y * 1 / 3 / 2 });
        if (ImGui::Begin("Inspector##Panel")) {
            //if (ImGui::CollapsingHeader("filesequence", ImGuiTreeNodeFlags_DefaultOpen | ImGuiWindowFlags_NoDocking)) {
            //    filesequence_node.onGui();
            //}
            if (ImGui::CollapsingHeader("read", ImGuiTreeNodeFlags_DefaultOpen)) {
                read_node.onGUI();
            }
            if (ImGui::CollapsingHeader("viewport", ImGuiTreeNodeFlags_DefaultOpen)) {
                viewport_node.onGUI();
            }
        }
        ImGui::End();

        if (IS_PLAYING)
        {
            int frame = read_node.frame.get();
            
            frame+=1;
            if (read_node.last_frame() < frame) {
                frame = read_node.first_frame();
            }
            read_node.frame.set(frame);
        }

        glazy::end_frame();

        FrameMark;
    }
    glazy::destroy();

}
