#pragma once

#include "./nodes.h"
#include "imgui.h"
#include "../ImGuiWidgets.h"
#include "glad/glad.h"

class ViewportNode {
private:

    GLuint _fbo;
    GLuint _program;

    GLint glinternalformat = GL_RGBA16F;


    enum class DeviceTransform : int {
        Linear = 0, sRGB = 1, Rec709 = 2
    };

    ImVec2 pan{ 0,0 };
    float zoom{ 1 };

public:
    Nodes::Inlet<std::tuple<void*, TextureSpec>> image_in{ "image_in" };
    Nodes::Attribute<float> gamma{ 1.0 };
    Nodes::Attribute<float> gain{ 0.0 };
    Nodes::Attribute<DeviceTransform> selected_device{ DeviceTransform::sRGB };

    GLuint _datatex = -1;
    GLuint _correctedtex = -1;
    int _width = 0;
    int _height = 0;
    GLenum _glformat; // keep for gui
    GLenum _gltype; // keep for gui

    ViewportNode()
    {
        init_shader();
        image_in.onTrigger([&](std::tuple<void*, TextureSpec> data_plate) {

            ZoneScopedN("upload to texture");
            auto [data, plate] = data_plate;
            assert((data != nullptr, "data must hold values"));

            // update texture size and bounding box
            if (_width != plate.width || _height != plate.height || _glformat != plate.glformat || _gltype != plate.gltype) {
                _width = plate.width;
                _height = plate.height;
                _glformat = plate.glformat;
                _gltype = plate.gltype;
                init_textures();
            }

            update_textures(data, plate.width, plate.height, plate.glformat, plate.gltype);
            color_correct_texture();
            });

        gamma.onChange([&](auto val) {color_correct_texture(); });
        gain.onChange([&](auto val) {color_correct_texture(); });
    }

    void init_textures()
    {
        std::cout << "Viewport->init texture" << "\n";
        glinternalformat = internalformat_from_format_and_type(_glformat, _gltype);
        if (glIsTexture(_datatex)) glDeleteTextures(1, &_datatex);
        glGenTextures(1, &_datatex);
        glBindTexture(GL_TEXTURE_2D, _datatex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, _width, _height);
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


    void update_textures(void* ptr, int w, int h, GLenum glformat, GLenum gltype) {
        // transfer pixels to texture
        //auto [x, y, w, h] = m_bbox;
        glBindTexture(GL_TEXTURE_2D, _datatex);
        //glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, gltype, ptr);
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

            ImGui::LabelText("glformat", "%s", to_string(_glformat).c_str());
            ImGui::LabelText("gltype", "%s", to_string(_gltype).c_str());

            // select internal texture format
            std::vector<GLenum> internal_format_options = { GL_RGB8, GL_RGBA8, GL_RGB16F, GL_RGBA16F, GL_RGB32F, GL_RGBA32F };
            std::vector<std::string> format_names;
            for (auto format : internal_format_options)
            {
                format_names.push_back(to_string(format));
            }

            auto it = std::find(internal_format_options.begin(), internal_format_options.end(), glinternalformat);
            int current_index = -1;
            if (it != internal_format_options.end()) {
                current_index = std::distance(internal_format_options.begin(), it);
            }
            if (ImGui::Combo("glinternalformat", &current_index, format_names)) {
                glinternalformat = internal_format_options[current_index];
                init_textures();
            }
            //ImGui::SetNextItemWidth(devices_combo_width);
            //ImGui::Combo("##device", &selected_device, "linear\0sRGB\0Rec.709\0");
            //if (ImGui::IsItemHovered()) ImGui::SetTooltip("device");

            ImGui::ImageViewer("viewer1", (ImTextureID)_correctedtex, _width, _height, &pan, &zoom);
        }
        ImGui::EndGroup();
    }
};
