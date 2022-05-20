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

    void ImageViewer(ImTextureID user_texture_id, glm::ivec2 res, ImVec2 size, ImVec2* pan, float* zoom, ImVec4 tint_col = ImVec4(1, 1, 1, 1), ImVec4 border_col = ImVec4(0, 0, 0, 0)) {
        // get item rect
        auto itempos = ImGui::GetCursorPos(); // if child window has no border this is: 0,0
        auto itemsize = ImGui::CalcItemSize(size, 540, 360);

        ImVec2 itemmouse = ImGui::GetMousePos() - itempos - ImGui::GetWindowPos();
        ImVec2 dim(res.x, res.y);

        static Camera camera;

        ImGui::BeginGroup();
        ImGui::InvisibleButton("camera control", itemsize);
        if (ImGui::IsItemActive())
        {

            if (ImGui::IsMouseDragging(0) || ImGui::IsMouseDragging(2))// && !ImGui::GetIO().KeyMods)
            {
                ImVec2 offset = ImVec2(ImGui::GetIO().MouseDelta.x, ImGui::GetIO().MouseDelta.y);
                std::cout << "mousedelta: " << offset.x << offset.y << "\n";
                *pan -= offset;

                camera.pan(-ImGui::GetIO().MouseDelta.x / itemsize.x, -ImGui::GetIO().MouseDelta.y / itemsize.y);
            }
        }

        if (ImGui::IsItemHovered()) {
            if (ImGui::GetIO().MouseWheel != 0 && !ImGui::GetIO().KeyMods)
            {
                float zoom_factor = 1.0 + (-ImGui::GetIO().MouseWheel * 0.2);
                *zoom /= zoom_factor;
                const auto target_distance = camera.get_target_distance();
                camera.dolly(-ImGui::GetIO().MouseWheel * target_distance * 0.2);
            }
        }

        // init render to texture
        static GLuint colorattachment = imdraw::make_texture(itemsize.x, itemsize.y, NULL);
        static GLuint fbo = imdraw::make_fbo(colorattachment);
        {


            // begin render to tewxture
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, itemsize.x, itemsize.y);

            // draw to fbo
            glClearColor(0, 0, 0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            camera.aspect = itemsize.x / itemsize.y;
            imdraw::set_projection(camera.getProjection());
            imdraw::set_view(camera.getView());
            imdraw::quad((GLuint)user_texture_id);

            // end render to texture
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        ImGui::SetCursorPos(itempos);
        ImGui::Image((ImTextureID)colorattachment, itemsize, { 0,1 }, {1,0}, tint_col, border_col);
        ImGui::Text("pan: %f,%f", pan->x, pan->y);
        ImGui::Text("zoom: %f", *zoom);

        ImGui::Text("itemmouse: %f,%f", itemmouse.x, itemmouse.y);
        ImGui::Text("itemsize: %f, %f", itemsize.x, itemsize.y);
        if (ImGui::Button("reset"))
        {
            pan->x = 0;
            pan->y = 0;
            *zoom = 1.0;
        }
        //ImGui::Text("itemmouse: %f, %f", itemmouse.x, itemmouse.y);

        ImGui::EndGroup();
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (glIsTexture(_correctedtex)) glDeleteTextures(1, &_correctedtex);
        glGenTextures(1, &_correctedtex);
        glBindTexture(GL_TEXTURE_2D, _correctedtex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

    void onGUI() {

        ImGui::Text("gain %f", gain.get());
        ImGui::Text("gamma %f", gamma.get());
        ImGui::Text("gain %f", gain.get());

        if (ImGui::Begin("viewport")) {
            ImGui::Text("viewport");

            static ImVec2 pan{ 0,0 };
            static float zoom{ 1 };
            ImGui::ImageViewer((ImTextureID)_datatex, {width, height}, { -1, 0}, &pan, &zoom, { 1,1,1,1 }, { 1,1,1,0.3 });
        }
        ImGui::End();
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
    glazy::set_vsync(false);
    auto filesequence_node = FilesequenceNode();
    auto read_node = ReadNode();
    auto viewport_node = ViewportNode();
    filesequence_node.out.target(read_node.filename_in);
    read_node.plate_out.target(viewport_node.image_in);

    filesequence_node.open("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");

    while (glazy::is_running()) {
        glazy::new_frame();
        FrameMark;
        if (ImGui::Begin("Inspector")) {
            if (ImGui::CollapsingHeader("filesequence", ImGuiTreeNodeFlags_DefaultOpen)) {
                filesequence_node.onGui();
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
