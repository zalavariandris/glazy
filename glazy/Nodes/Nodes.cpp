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
#include "FileSequence.h";

#include <filesystem>
#include <tuple>

#include "OpenImageIO/imageio.h"

#include "glad/glad.h"

#include "../../tracy/Tracy.hpp"
#include "Camera.h"

#include <format>

template<typename T> class MyInlet;
template<typename T> class MyOutlet;

class Node;

template <typename T>
class MyInlet
{

public:
    void onTrigger(std::function<void(T)> handler)
    {
        handlers.push_back(handler);
    }

    template<typename T> friend class MyOutlet;
    ~MyInlet() {
        for (MyOutlet<T>* outlet : sources)
        {
            auto it = std::find(outlet->targets.begin(), outlet->targets.end(), this);
            if (it!=outlet->targets.end()) {
                outlet->targets.erase(it);
            }
        }
    }
private:
    std::vector<std::function<void(T)>> handlers;
    std::vector<MyOutlet<T>*> sources;
    friend class MyOutlet<T>;


};


template <typename T>
class MyOutlet {
private:
    T value;
    std::vector<const MyInlet<T>*> targets;
public:
    

    void target(const MyInlet<T>& inlet) {
        targets.push_back(&inlet);
        //inlet.sources.push_back(this);
        for (const auto& handler : inlet.handlers)
        {
            handler(value);
        }
    }

    void trigger(T props) {
        value = props;
        for (const auto& inlet : targets) {
            for (const auto& handler : inlet->handlers)
            {
                handler(value);
            }
        }
    }

    //~MyOutlet() {
    //    for (const MyInlet<T>* inlet : targets)
    //    {
    //        auto it = std::find(inlet->sources.begin(), inlet->sources.end(), this);
    //        if (it != inlet->sources.end()) {
    //            inlet->sources.erase(it);
    //        }
    //    }
    //}

    friend class MyInlet<T>;
};



template <typename T>
class Attribute {
    T value;
    std::vector<std::function<void(T)>> handlers;
public:
    void set(T val) {
        for (const auto& handler : handlers) {
            handler(val);
        }
        value = val;
    }

    const T& get() const
    {
        return value;
    }

    void onChange(std::function<void(T)> handler)
    {
        handlers.push_back(handler);
    }
};

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

namespace ImGui {
    bool Attrib(const char* label, Attribute<int>* attr, int v_min, int v_max)
    {
        int val = attr->get();
        if (ImGui::SliderInt(label, &val, v_min, v_max)) {
            attr->set(val);
            return true;
        }
        return false;
    }

    void ImageViewer(const char* std_id, ImTextureID user_texture_id, glm::ivec2 res, ImVec2 size, ImVec2* pan, float* zoom, float zoom_speed = 0.1, ImVec4 tint_col = ImVec4(1, 1, 1, 1), ImVec4 border_col = ImVec4(0, 0, 0, 0.3)) {
        auto itemsize = ImGui::CalcItemSize(size, 540 / 2, 360 / 2);

        // get item rect
        auto windowpos = ImGui::GetWindowPos();

        // Projection
        ImVec4 viewport(0, 0, itemsize.x, itemsize.y);

        auto matrices = [&](ImVec2 pan, float zoom)->std::tuple<glm::mat4, glm::mat4, glm::mat4> {
            glm::mat4 projection = glm::ortho(-itemsize.x/2, itemsize.x/2, -itemsize.y/2, itemsize.y/2, 0.1f, 10.0f);
            glm::mat4 view(1);
            glm::mat4 model = glm::mat4(1);
            view = glm::translate(view, { pan.x, pan.y, 0.0f });
            view = glm::scale(view, { 1.0 / zoom, 1.0 / zoom, 1.0 / zoom });
            return { model, view, projection };
        };

        // world to screen
        auto project = [&](ImVec2 world)->ImVec2
        {
            auto [model, view, projection] = matrices(*pan, *zoom);
            auto screen = glm::project(glm::vec3(world.x, world.y, 0.0f), model * glm::inverse(view), projection, glm::uvec4(viewport.x, viewport.y, viewport.z, viewport.w));
            return { screen.x, screen.y };
        };

        // screen to world
        auto unproject = [&](ImVec2 screen)->ImVec2
        {
            auto [model, view, projection] = matrices(*pan, *zoom);
            auto world = glm::unProject(glm::vec3(screen.x, screen.y, 0.0f), model * glm::inverse(view), projection, glm::uvec4(viewport.x, viewport.y, viewport.z, viewport.w));
            return { world.x, world.y };
        };

        const auto set_zoom = [&](float value, ImVec2 pivot) {
            auto mouse_world = unproject(pivot);
            *zoom = value;
            *pan += mouse_world - unproject(pivot);
        };

        // GUI
        ImGui::PushID((ImGuiID)std_id);
        ImGui::BeginGroup();

        auto itempos = ImGui::GetCursorPos();
        auto mouse_item = ImGui::GetMousePos() - itempos - windowpos + ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
        auto mouse_item_prev = ImGui::GetMousePos() - ImGui::GetIO().MouseDelta - itempos - windowpos + ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
        ImGui::InvisibleButton("camera control", itemsize);
        ImGui::SetItemAllowOverlap();
        ImGui::SetItemUsingMouseWheel();
        if (ImGui::IsItemActive())
        {
            if (ImGui::IsMouseDragging(0) || ImGui::IsMouseDragging(2))// && !ImGui::GetIO().KeyMods)
            {
                ImVec2 offset = unproject(mouse_item_prev) - unproject(mouse_item);
                *pan += offset;
            }
        }

        if (ImGui::IsItemHovered()) {
            if (ImGui::GetIO().MouseWheel != 0 && !ImGui::GetIO().KeyMods)
            {
                //auto mouse_world = unproject(mouse_item_prev);
                float zoom_factor = 1.0 + (-ImGui::GetIO().MouseWheel * zoom_speed);
                //*zoom /= zoom_factor;
                //*zoom /= zoom_factor;

                //ImVec2 offset = mouse_world - unproject(mouse_item);
                //*pan += offset;

                set_zoom(*zoom / zoom_factor, mouse_item);


            }
        }
        ImGui::SetCursorPos(itempos);

        auto bl = project(ImVec2(0,0));
        auto tr = project(ImVec2(res[0], res[1]));


        /// Render to Texture
        //RenderTexture render_to_texture = RenderTexture(itemsize.x, itemsize.y);
        //render_to_texture.resize(itemsize.x, itemsize.y);
        //render_to_texture([&]() {
        //    imdraw::set_projection(glm::ortho(0.0f, itemsize.x, 0.0f, itemsize.y));
        //    imdraw::set_view(glm::mat4(1));
        //    imdraw::quad((GLuint)user_texture_id, { bl.x, bl.y }, { tr.x, tr.y });


        //    imdraw::disc({ mouse_item.x, mouse_item.y,0.0f}, 10);

        //});
        //ImGui::Image((ImTextureID)render_to_texture.color(), itemsize, {0,0}, {1,1}, tint_col, border_col);

        /// Frame with UV coods
        ImVec2 uv0 = (ImVec2(0,0) - bl) / (tr - bl);
        ImVec2 uv1 = (itemsize - bl) / (tr - bl);
        ImGui::Image((ImTextureID)user_texture_id, itemsize, uv0, uv1, tint_col, { 0,0,0,0 });

        {// Image info
            ImGui::PushClipRect(itempos + windowpos, windowpos + itempos + itemsize, true);
            auto draw_list = ImGui::GetWindowDrawList();
            ImRect image_data_rect = ImRect(project(ImVec2(0, 0)), project(ImVec2(res[0], res[1])));
            draw_list->AddRect(ImGui::GetWindowPos() + itempos + image_data_rect.Min, ImGui::GetWindowPos() + itempos + image_data_rect.Max, ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]), 0.0);

            std::string str;
            int width = res[0];
            int height = res[1];
            //ImGui::SetCursorPos(itempos + image_data_rect.Max);
            if (width == 1920 && height == 1080)
            {
                str = "HD";
            }
            else if (width == 3840 && height == 2160)
            {
                str = "UHD 4K";
            }
            else if (width == height)
            {
                str = "Square %d";
            }
            else
            {
                str = fmt::sprintf("%dx%d", width, height);
            }

            draw_list->AddText(windowpos + itempos + image_data_rect.Max, ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), str.c_str());

            draw_list->AddRect(windowpos + itempos, windowpos + itempos + itemsize, ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]));
            ImGui::PopClipRect();
        }

        /// Overlay toolbar
        {
            ImGui::SetCursorPos(itempos);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);

            ImGui::SetNextItemWidth(ImGui::CalcTextSize("100%").x + ImGui::GetStyle().FramePadding.x * 2);
            int zoom_percent = *zoom * 100;
            if (ImGui::DragInt("##zoom", &zoom_percent, 0.1, 1, 100, "%d%%", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_Vertical)) {
                set_zoom(zoom_percent / 100.0, itemsize/2);
            }

            if (ImGui::IsItemClicked(1)) {
                *zoom = 1.0;
            }
            ImGui::SameLine();
            ImGui::Button("pan", { 0,0 });
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                std::cout << "item active" << "\n";
                ImVec2 offset = unproject(mouse_item_prev) - unproject(mouse_item);
                *pan += offset;
            }
            if (ImGui::IsItemClicked(1))
            {
                *pan = ImVec2(res[0] / 2, res[1] / 2);
            }
            ImGui::SameLine();
            if (ImGui::Button("fit")) {
                *zoom = std::min(itemsize.x / res[0], itemsize.y / res[1]);
                *pan = ImVec2(res[0] / 2, res[1] / 2);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        ImGui::EndGroup();
        ImGui::PopID();
    }
}

class FilesequenceNode
{
    FileSequence sequence;

public:
    Attribute<std::filesystem::path> pattern;
    Attribute<int> frame;
    Attribute<bool> play;
    MyOutlet<std::filesystem::path> out;

    FilesequenceNode()
    {
        pattern.onChange([&](auto filepath){
            parse();
        });

        frame.onChange([&](int F) {
            parse();
        });
    }

    void open(std::filesystem::path filename = std::filesystem::path())
    {
        if (filename.empty()) {
            filename = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg");
        }
        sequence = FileSequence(filename);
        frame.set(sequence.first_frame);
        pattern.set(sequence.pattern);
    }

    void parse() {
        auto result = sequence.item(frame.get());
        out.trigger(result);
    }

    void onGui()
    {
        if(ImGui::Button("open"))
        {
            open();
        }
        ImGui::LabelText("pattern", "%s", pattern.get().string().c_str());
        ImGui::LabelText("range", "[%d-%d]", sequence.first_frame, sequence.last_frame);

        if (ImGui::Button(play.get() ? "pause" : "play")) {
            play.set(play.get() ? false : true);
            std::cout << "change play to: " << play.get() << "\n";
        }

        if (ImGui::Attrib("frame", &frame, sequence.first_frame, sequence.last_frame)) {
            play.set(false);
        };

        /*
        int _frame_ = frame.get();
        if (ImGui::SliderInt("frame", &_frame_, sequence.first_frame, sequence.last_frame))
        {
            play.set(false);
            frame.set(_frame_);
        }
        */

        if (play.get()) {
            auto new_frame = frame.get();
            new_frame++;
            if (new_frame > sequence.last_frame) {
                new_frame = sequence.first_frame;
            }
            frame.set(new_frame);
        }

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open", "")) {
                    open();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
};

class ReadNode
{
private:
    std::filesystem::path filename;
    void* memory=NULL;
public:
    MyInlet<std::filesystem::path> filename_in;
    MyInlet<void*> memory_in;
    MyOutlet<std::tuple<void*, std::tuple<int, int, int,int>>> plate_out;

    std::vector<half> pixels;

    ReadNode()
    {
        filename_in.onTrigger([this](auto path) {
            filename = path;
            read();
        });

        memory_in.onTrigger([this](auto buffer) {
            memory = buffer;
            read();
        });
    }

    void read()
    {
        
        OIIO::ImageSpec spec;
        {
            ZoneScoped;
            //if (memory == NULL) return;
            if (!std::filesystem::exists(filename)) return;

            // read file to pixels
            auto file = OIIO::ImageInput::open(filename.string());
            spec = file->spec();
            int nchannels = 4;

            auto npixels = spec.width * spec.height * nchannels;
            pixels.reserve(npixels);
            if (pixels.size() < npixels) {
                std::cout << "resize pixels" << "\n";
                pixels.resize(npixels);
            }
            file->read_image(0, nchannels, OIIO::TypeHalf, &pixels[0]);
            file->close();
        }
        plate_out.trigger({ &pixels[0],{spec.x,spec.y,spec.width,spec.height} });
    }

    void onGUI() {
        ImGui::LabelText("filename", "%s", filename.string().c_str());
    }
};


class ViewportNode {
private:
    GLuint _datatex=-1;
    GLuint _fbo;
    GLuint _program;
    GLuint _correctedtex = -1;
    GLint glinternalformat = GL_RGBA16F;

    int width=0;
    int height=0;

    enum class DeviceTransform : int{
        Linear=0, sRGB=1, Rec709=2
    };

    ImVec2 itemsize;
    ImVec2 pan{ 0,0 };
    float zoom{ 1 };
public:
    MyInlet<std::tuple<void*, std::tuple<int, int,int,int>>> image_in;
    Attribute<float> gamma;
    Attribute<float> gain;
    Attribute<DeviceTransform> selected_device;

    ViewportNode()
    {
        init_shader();
        image_in.onTrigger([&](auto plate) {
            
            ZoneScopedN("upload to texture");
            auto [ptr, bbox] = plate;
            auto [x, y, w, h] = bbox;

            // update texture size and bounding box
            if (width != w || height != h) {
                width = w;
                height = h; 
                init_texture();
            }

            update_texture(ptr, w, h);
            color_correct_texture();
        });
    }

    void init_texture()
    {
        std::cout << "init texture" << "\n";

        if (glIsTexture(_datatex)) glDeleteTextures(1, &_datatex);
        glGenTextures(1, &_datatex);
        glBindTexture(GL_TEXTURE_2D, _datatex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
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
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void update_texture(void* ptr, int w, int h) {
        // update texture
        auto glformat = GL_RGBA;

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
    
        _fbo = imdraw::make_fbo(_correctedtex);
    }

    void color_correct_texture()
    {
        // begin render to tewxture
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glViewport(0, 0, width, height);

        // draw to fbo
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        imdraw::push_program(_program);
        /// Draw quad with fragment shader
        imgeo::quad();
        static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
        static auto vao = imdraw::make_vao(_program, { {"aPos", {vbo, 3}} });

        imdraw::set_uniforms(_program, {
            {"inputTexture", 0},
            {"resolution", glm::vec2(width, height)},
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

    void fit()
    {

        zoom = std::min(itemsize.x / width, itemsize.y / height);
        pan = ImVec2(width / 2, height / 2);
    }

    void onGUI() {

        ImGui::LabelText("gain",  "%f", gain.get());
        ImGui::LabelText("gamma", "%f", gamma.get());
        ImGui::LabelText("gain",  "%f", gain.get());


        ImVec2 itemsize;
        if (ImGui::Begin("Viewport"))
        {
            ImGui::ImageViewer("viewer1", (ImTextureID)_datatex, {width, height}, {-1, -1}, &pan, &zoom, 0.1);
            itemsize = ImGui::GetItemRectSize();
        }
        ImGui::End();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("Reset Zoom", "")) {
                    zoom = 1.0;
                }
                if (ImGui::MenuItem("Fit", "")) {
                    fit();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
};

int main()
{
    auto outlet = MyOutlet<int>();
    auto inlet = MyInlet<int>();
    
    inlet.onTrigger([](int props) {
        std::cout << "inlet got value: " << props << "\n";
    });

    outlet.target(inlet);
    outlet.trigger(5);
    outlet.trigger(10);
    
    std::cout << "Hello World!\n";

    glazy::init();
    glazy::set_vsync(true);
    auto filesequence_node = FilesequenceNode();
    auto read_node = ReadNode();
    auto viewport_node = ViewportNode();
    filesequence_node.out.target(read_node.filename_in);
    read_node.plate_out.target(viewport_node.image_in);

    filesequence_node.open("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");
    //viewport_node.fit();

    while (glazy::is_running()) {
        glazy::new_frame();
        FrameMark;
        if (ImGui::Begin("Inspector")) {
            if (ImGui::CollapsingHeader("filesequence", ImGuiTreeNodeFlags_DefaultOpen)) {
                filesequence_node.onGui();
                filesequence_node.pattern.onChange([&](auto pattern) {
                    //viewport_node.fit();
                });
            }
            if (ImGui::CollapsingHeader("read", ImGuiTreeNodeFlags_DefaultOpen)) {
                read_node.onGUI();
            }
            if (ImGui::CollapsingHeader("viewport", ImGuiTreeNodeFlags_DefaultOpen)) {
                viewport_node.onGUI();
            }
        }
        ImGui::End();

        glazy::end_frame();
        FrameMark;
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
