#pragma once

#include "glad/glad.h"
#include <tuple>
#include <map>
#include <string>
#include "imdraw/imdraw_internal.h"

class PixelsRenderer
{
public:
    GLuint fbo{ 0 };
    GLuint color_attachment{ 0 };
    GLuint mProgram;

    GLuint data_tex;
    int width, height;
    std::tuple<int, int, int, int> m_bbox;


    GLenum glinternalformat = GL_RGBA16F; // select texture internal format

    PixelsRenderer(int width, int height);

    void onGUI();

    void init_tex(int width, int height);

    void init_fbo(int width, int height);

    void init_program();

    void set_uniforms(std::map<std::string, imdraw::UniformVariant> uniforms);

    void render_texture_to_fbo();

    // write texture from memory
    void update_from_data(void* pixels, std::tuple<int, int, int, int> bbox, std::vector<std::string> channels, GLenum gltype = GL_HALF_FLOAT);

    // write texture from pbo
    void update_from_pbo(GLuint pbo, const std::tuple<int, int, int, int>& bbox, const std::vector<std::string>& channels, GLenum gltype = GL_HALF_FLOAT);
};