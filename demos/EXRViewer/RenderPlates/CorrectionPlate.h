#pragma once

#include <map>
#include <string>
#include "imdraw/imdraw_internal.h"
#include "glad/glad.h"


class CorrectionPlate
{
private:
    bool autoreload{ true };
    GLuint mInputtex;
    GLuint fbo;
    GLuint color_attachment;
    int mWidth;
    int mHeight;
    GLuint mProgram;

    int selected_device{ 1 };
    float gain{ 0 };
    float gamma{ 1.0 };


public:
    /// FBO size, and input texture id
    CorrectionPlate(int width, int height, GLuint inputtex);

    void set_uniforms(std::map<std::string, imdraw::UniformVariant> uniforms);

    /// resize FBO
    void resize(int width, int height);

    void onGui();

    void set_input_tex(GLuint tex);

    void compile_program();

    void update();

    void render();
};