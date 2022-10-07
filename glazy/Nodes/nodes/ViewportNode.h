#pragma once

#include "./nodes.h"
#include "glad/glad.h"
#include "MemoryImage.h"
#include <imgui.h>

class ViewportNode
{
private:

    GLuint _fbo;
    GLuint _program;

    GLint glinternalformat = GL_RGBA16F;

    ImVec2 pan{ 0,0 };
    float zoom{ 1 };

public:
    enum class DeviceTransform : int {
        Linear = 0, sRGB = 1, Rec709 = 2
    };

    enum class AlphaChannelMode : int {
        Opaque = 0, Transparent = 1, Overlay = 2
    };

    Nodes::Inlet<std::shared_ptr<MemoryImage>> image_in{ "image_in" };
    Nodes::Attribute<float> gamma{ 1.0 };
    Nodes::Attribute<float> gain{ 0.0 };
    Nodes::Attribute<DeviceTransform> selected_device{ DeviceTransform::sRGB };
    Nodes::Attribute<AlphaChannelMode> alpha_channel_mode{ AlphaChannelMode::Opaque };

    GLuint _datatex = -1;
    GLuint _correctedtex = -1;
    int _width = 0;
    int _height = 0;
    GLenum _glformat; // keep for gui
    GLenum _gltype; // keep for gui

    ViewportNode();

    void init_textures();


    void update_textures(void* ptr, int w, int h, GLenum glformat, GLenum gltype);

    void init_shader();

    void color_correct_texture();

    void onGUI();
};
