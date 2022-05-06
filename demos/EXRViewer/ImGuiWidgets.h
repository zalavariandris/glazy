#pragma once
// Icon Fonts

#include "imgui.h"
#include <vector>
#include <string>
#include "Camera.h"
#include "glad/glad.h"

namespace ImGui
{
    bool Frameslider(const char* label, bool* is_playing, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);

    int CalcComboWidth(const std::vector<std::string>& values, ImGuiComboFlags flags = 0);

    bool CameraControl(Camera* camera, ImVec2 item_size);

    struct GLViewerState {
        GLuint viewport_fbo;
        GLuint viewport_color_attachment;
        Camera camera{ { 0,0,5000 }, { 0,0,0 }, { 0,1,0 } };
    };

    void BeginGLViewer(GLuint* fbo, GLuint* color_attachment, Camera* camera, const ImVec2& size_arg = ImVec2(0, 0));

    void EndGLViewer();
}