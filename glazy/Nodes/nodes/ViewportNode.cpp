#include "ViewportNode.h"

#include <IconsFontAwesome5.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include "../ImGuiWidgets.h"

//#include "../glazy/glazy.h"
#include "../glazy/imdraw/imdraw.h"
#include "../glazy/imdraw/imdraw_internal.h"
#include "../glazy/imgeo/imgeo.h"
#include "../glazy/glhelpers.h"

#include "../glazy/glazy.h"

ViewportNode::ViewportNode()
{
    init_shader();
    image_in.onTrigger([&](std::shared_ptr<MemoryImage> image) {
        if (!image) return;
        //ZoneScopedN("upload to texture");
        // update texture size and bounding box
        if (_width != image->width || _height != image->height || _glformat != image->glformat() || _gltype != image->gltype()) {
            _width = image->width;
            _height = image->height;
            _glformat = image->glformat();
            _gltype = image->gltype();
            init_textures();
        }

        assert((image->data != nullptr, "data must hold values"));
        update_textures(image->data, image->width, image->height, image->glformat(), image->gltype());
        color_correct_texture();
        });

    gamma.onChange([&](auto val) {color_correct_texture(); });
    gain.onChange([&](auto val) {color_correct_texture(); });
    selected_device.onChange([&](auto val) {color_correct_texture(); });
    alpha_channel_mode.onChange([&](auto val) {color_correct_texture(); });
}

void ViewportNode::init_textures()
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

void ViewportNode::update_textures(void* ptr, int w, int h, GLenum glformat, GLenum gltype) {
    // transfer pixels to texture
    //auto [x, y, w, h] = m_bbox;
    glBindTexture(GL_TEXTURE_2D, _datatex);
    //glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, gltype, ptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ViewportNode::init_shader()
{
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
    uniform int alpha_channel_mode; // 0:opaque | 1:transparent | 2:matte overlay

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

        float alpha = 1.0;
        
        if(alpha_channel_mode == 0){
            alpha = 1.0;
        }
        else if(alpha_channel_mode==1)
        {
            alpha = texture(inputTexture, uv).a;
        }
        else if(alpha_channel_mode==2)
        {
            alpha=1.0;
            color.r+=texture(inputTexture, uv).a;
        }

        FragColor = vec4(color, alpha);
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

void ViewportNode::color_correct_texture()
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
        {"alpha_channel_mode",(int)alpha_channel_mode.get()}
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

void ViewportNode::onGUI()
{

    ImVec2 itemsize;
    ImGui::BeginGroup(); // display correction
    {
        static const std::vector<std::string> devices{ "linear", "sRGB", "Rec.709" };
        int devices_combo_width = ImGui::CalcComboWidth(devices);



        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 6);
        ImGui::SliderFloat(ICON_FA_ADJUST "##gain", &gain, -6.0f, 6.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("gain");
        if (ImGui::IsItemClicked(1)) gain.set(0.0);

        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 6);
        ImGui::SliderFloat("##gamma", &gamma, 0, 4);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("gamma");
        if (ImGui::IsItemClicked(1)) gamma.set(1.0);

        static const std::vector<std::string> alpha_modes = { "opaque", "transparent", "matte overlay" };
        int alpha_combo_width = ImGui::CalcComboWidth(alpha_modes);
        ImGui::SetNextItemWidth(alpha_combo_width);
        int selected_alpha_idx = (int)alpha_channel_mode.get();
        if (ImGui::Combo("##alpha_mode", &selected_alpha_idx, "opaque\0transparent\0matte\0")) {
            alpha_channel_mode.set((AlphaChannelMode)selected_alpha_idx);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("alpha channel mode");

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


        if (ImGui::Combo("glinternalformat", &current_index, format_names))
        {
            glinternalformat = internal_format_options[current_index];
            init_textures();
        }

        ImGui::SetNextItemWidth(devices_combo_width);
        int selected_device_idx = (int)selected_device.get();
        if (ImGui::Combo("##device", &selected_device_idx, "linear\0sRGB\0Rec.709\0")) {
            selected_device.set((DeviceTransform)selected_device_idx);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("device");

        //ImGui::ImageViewer("viewer1", (ImTextureID)_correctedtex, _width, _height, &pan, &zoom);

    }
    ImGui::EndGroup();
}