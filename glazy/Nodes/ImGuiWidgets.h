#pragma once
// Icon Fonts

#include "imgui.h"
#include <string>
#include <vector>
#include "nodes.h"

namespace ImGui
{
    void ImageViewer(const char* std_id, ImTextureID user_texture_id, int tex_width, int tex_height, ImVec2* pan, float* zoom, ImVec2 size={0,0}, float zoom_speed = 0.1, ImVec4 tint_col = ImVec4(1, 1, 1, 1), ImVec4 border_col = ImVec4(0, 0, 0, 0.3));

    void Ranges(const std::vector<std::tuple<int, int>>& ranges, int v_min, int v_max, ImVec2 size = ImVec2(-1, 20));

    bool Frameslider(const char* label, bool* is_playing, int* v, int v_min, int v_max, const std::vector<std::tuple<int, int>>& ranges={}, const char* format = "%d", ImGuiSliderFlags flags = 0);

    int CalcComboWidth(const std::vector<std::string>& values, ImGuiComboFlags flags = 0);
}

namespace ImGui {
    bool SliderInt(const char* label, Nodes::Attribute<int>* attr, int v_min, int v_max);

    bool SliderFloat(const char* label, Nodes::Attribute<float>* attr, int v_min, int v_max);
}