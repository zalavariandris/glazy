#include "PixelsRenderer.h"

#include "imgui.h"

#include "helpers.h" // BeginRenderToTexture, to_string

#include <iostream>

PixelsRenderer::PixelsRenderer(int width, int height) : width(width), height(height)
{
    init_tex(width, height);
    init_fbo(width, height);
    init_program();
}

void PixelsRenderer::onGUI()
{
    /// Internalformat
    //if (ImGui::BeginListBox("texture internal format"))
    //{
    //    std::vector<GLenum> glinternalformats{ GL_RGB16F, GL_RGBA16F, GL_RGB32F, GL_RGBA32F };
    //    for (auto i = 0; i < glinternalformats.size(); ++i)
    //    {
    //        const bool is_selected = glinternalformat == glinternalformats[i];
    //        if (ImGui::Selectable(to_string(glinternalformats[i]).c_str(), is_selected))
    //        {
    //            glinternalformat = glinternalformats[i];
    //            init_tex(width, height);
    //        }
    //    }
    //    ImGui::EndListBox();
    //}
}

void PixelsRenderer::init_tex(int width, int height)
{
    // init texture object
    //glPrintErrors();
    glGenTextures(1, &data_tex);
    glBindTexture(GL_TEXTURE_2D, data_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "init tex errors:" << "\n";
    //glPrintErrors();
}

void PixelsRenderer::init_fbo(int width, int height)
{
    color_attachment = imdraw::make_texture_float(width, height, 0, glinternalformat, GL_BGRA, GL_HALF_FLOAT);
    fbo = imdraw::make_fbo(color_attachment);
}

void PixelsRenderer::init_program() {
    //ZoneScoped;
    std::cout << "recompile checker shader" << "\n";

    // reload shader code
    auto vertex_code = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        void main()
        {
            gl_Position = vec4(aPos, 1.0);
        }
    )";

    auto fragment_code = R"(
        #version 330 core
        out vec4 FragColor;
        uniform mediump sampler2D inputTexture;
        uniform vec2 resolution;
        uniform ivec4 bbox;

        void main()
        {
            vec2 uv = (gl_FragCoord.xy)/resolution; // normalize fragcoord
            uv=vec2(uv.x, 1.0-uv.y); // flip y
            uv-=vec2(bbox.x/resolution.x, bbox.y/resolution); // position image
            vec3 color = texture(inputTexture, uv).rgb;
            float alpha = texture(inputTexture, uv).a;
            FragColor = vec4(color, alpha);
        }
    )";

    // link new program
    GLuint program = imdraw::make_program_from_source(
        vertex_code,
        fragment_code
    );

    // swap programs
    if (glIsProgram(mProgram)) glDeleteProgram(mProgram);
    mProgram = program;
}

void PixelsRenderer::set_uniforms(std::map<std::string, imdraw::UniformVariant> uniforms) {
    imdraw::set_uniforms(mProgram, uniforms);
}

void PixelsRenderer::render_texture_to_fbo()
{
    //ZoneScopedN("datatext to fbo");
    BeginRenderToTexture(fbo, 0, 0, width, height);
    {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        auto [x, y, w, h] = m_bbox;
        set_uniforms({
            {"inputTexture", 0},
            {"resolution", glm::vec2(width, height)},
            {"bbox", glm::ivec4(x,y,w,h)}
            });

        /// Create geometry
        static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
        static auto vao = imdraw::make_vao(mProgram, { {"aPos", {vbo, 3}} });

        /// Draw quad with fragment shader
        imdraw::push_program(mProgram);
        glBindTexture(GL_TEXTURE_2D, data_tex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        imdraw::pop_program();
    }
    EndRenderToTexture();
}

// write texture from memory
void PixelsRenderer::update_from_data(void* pixels, std::tuple<int, int, int, int> bbox, std::vector<std::string> channels, GLenum gltype)
{
    //ZoneScopedN("pixels to texture");
    { // Upload from memory
        std::array<GLint, 4> swizzle_mask;
        auto glformat = glformat_from_channels(channels, swizzle_mask);
        if (glformat == -1) return;

        // update bounding box
        m_bbox = bbox;

        // transfer pixels to texture
        auto [x, y, w, h] = m_bbox;
        glBindTexture(GL_TEXTURE_2D, data_tex);
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, GL_HALF_FLOAT, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    {
        // Draw data_texture to bounding box
        render_texture_to_fbo();
    }
}

// write texture from pbo
void PixelsRenderer::update_from_pbo(GLuint pbo, const std::tuple<int, int, int, int>& bbox, const std::vector<std::string>& channels, GLenum gltype)
{
    { // upload from PBO
        std::array<GLint, 4> swizzle_mask;
        auto glformat = glformat_from_channels(channels, swizzle_mask);
        if (glformat == -1) return;

        // update bounding box
        m_bbox = bbox;

        // transfer PBO to texture
        glBindTexture(GL_TEXTURE_2D, data_tex);
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        auto [x, y, w, h] = m_bbox;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, GL_HALF_FLOAT, 0/*NULL offset*/); // orphaning
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    // Draw data_texture to bounding box
    {
        render_texture_to_fbo();
    }
}