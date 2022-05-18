#include "CorrectionPlate.h"
#include "imgui.h"
#include "IconsFontAwesome5.h"
#include "../ImGuiWidgets.h"
#include "../helpers.h"
#include "imgeo/imgeo.h"
#include "imdraw/imdraw.h"

/// FBO size, and input texture id
CorrectionPlate::CorrectionPlate(int width, int height, GLuint inputtex)
{
    mInputtex = inputtex;
    resize(width, height);
    compile_program();
}

void CorrectionPlate::set_uniforms(std::map<std::string, imdraw::UniformVariant> uniforms) {
    imdraw::set_uniforms(mProgram, uniforms);
}

/// resize FBO
void CorrectionPlate::resize(int width, int height)
{
    //ZoneScopedN("update correction fbo");
    if (glIsFramebuffer(fbo))
        glDeleteFramebuffers(1, &fbo);
    if (glIsTexture(color_attachment))
        glDeleteTextures(1, &color_attachment);

    color_attachment = imdraw::make_texture_float(width, height, NULL, GL_RGBA);
    fbo = imdraw::make_fbo(color_attachment);

    mWidth = width;
    mHeight = height;
}

void CorrectionPlate::onGui()
{
    ImGui::BeginGroup(); // display correction
    {
        static const std::vector<std::string> devices{ "linear", "sRGB", "Rec.709" };
        int devices_combo_width = ImGui::CalcComboWidth(devices);
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 6);
        ImGui::SliderFloat(ICON_FA_ADJUST "##gain", &gain, -6.0f, 6.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("gain");
        if (ImGui::IsItemClicked(1)) gain = 0.0;

        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 6);
        ImGui::SliderFloat("##gamma", &gamma, 0, 4);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("gamma");
        if (ImGui::IsItemClicked(1)) gamma = 1.0;


        ImGui::SetNextItemWidth(devices_combo_width);
        ImGui::Combo("##device", &selected_device, "linear\0sRGB\0Rec.709\0");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("device");

    }
    ImGui::EndGroup();
}

void CorrectionPlate::set_input_tex(GLuint tex) {
    mInputtex = tex;
}

void CorrectionPlate::compile_program()
{
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

    const char* PASS_THROUGH_VERTEX_CODE = R"(
                #version 330 core
                layout (location = 0) in vec3 aPos;
                void main()
                {
                    gl_Position = vec4(aPos, 1.0);
                }
                )";

    //ZoneScopedN("recompile display correction shader");
    if (glIsProgram(mProgram)) {
        glDeleteProgram(mProgram);
    }
    mProgram = imdraw::make_program_from_source(PASS_THROUGH_VERTEX_CODE, display_correction_fragment_code);
}


    void CorrectionPlate::evaluate()
    {
        // update result texture
        BeginRenderToTexture(fbo, 0, 0, mWidth, mHeight);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        imdraw::push_program(mProgram);
        /// Draw quad with fragment shader
        imgeo::quad();
        static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
        static auto vao = imdraw::make_vao(mProgram, { {"aPos", {vbo, 3}} });

        set_uniforms({
            {"inputTexture", 0},
            {"resolution", glm::vec2(mWidth, mHeight)},
            {"gain_correction", gain},
            {"gamma_correction", 1.0f / gamma},
            {"convert_to_device", selected_device},
            });
        glBindTexture(GL_TEXTURE_2D, mInputtex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        imdraw::pop_program();
        EndRenderToTexture();
    }

    void CorrectionPlate::render()
    {
        // draw result texture
        imdraw::quad(color_attachment, { 0,0 }, { mWidth, mHeight });
    }