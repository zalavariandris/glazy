#pragma once

#include "ImGuiWidgets.h"
#include "IconsFontAwesome5.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h" // SeparatorEx - Vertical separator

#include "helpers.h"
#include "imdraw/imdraw.h"
#include "imdraw/imdraw_internal.h"

bool ImGui::Frameslider(const char* label, bool* is_playing, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    bool changed = false;
    ImGui::BeginGroup();
    {
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_FAST_BACKWARD "##first frame")) {
            *v = v_min;
            changed = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_STEP_BACKWARD "##step backward")) {
            *v -= 1;
            if (*v < v_min) *v = v_max;
            changed = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(*is_playing ? ICON_FA_PAUSE : ICON_FA_PLAY "##play"))
        {
            *is_playing = !*is_playing;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_STEP_FORWARD "##step forward")) {
            *v += 1;
            if (*v > v_max) *v = v_min;
            changed = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_FAST_FORWARD "##last frame")) {
            *v = v_max;
            changed = true;
        }
    }
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // timeslider
    ImGui::BeginGroup();
    {
        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 2);
        ImGui::InputInt("##start frame", &v_min, 0, 0);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() * 2 - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SliderInt("##frame", v, v_min, v_max)) {
            changed = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight() * 2);
        ImGui::InputInt("##end frame", &v_max, 0, 0);
        ImGui::EndDisabled();
    }
    ImGui::EndGroup();

    return changed;
}

int ImGui::CalcComboWidth(const std::vector<std::string>& values, ImGuiComboFlags flags)
{
    int min_width = ImGui::GetTextLineHeight();
    for (const auto& layer : values)
    {
        auto item_width = ImGui::CalcTextSize(layer.c_str()).x;
        if (item_width > min_width) {
            min_width = item_width;
        }
    }
    min_width += ImGui::GetStyle().FramePadding.x * 2;
    if ((flags & ImGuiComboFlags_NoArrowButton) == 0) min_width += ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2;
    return min_width;
}

bool ImGui::CameraControl(Camera* camera, ImVec2 item_size)
{
    // Control Camera
    bool changed{ false };
    ImGui::InvisibleButton("##camera control", item_size);

    if (ImGui::IsItemActive()) {
        if (ImGui::IsMouseDragging(0) && (ImGui::GetIO().KeyMods == (ImGuiKeyModFlags_Ctrl | ImGuiKeyModFlags_Alt)))
        {
            camera->orbit(-ImGui::GetIO().MouseDelta.x * 0.006, -ImGui::GetIO().MouseDelta.y * 0.006);
            changed = true;
        }
        else if (ImGui::IsMouseDragging(0) || ImGui::IsMouseDragging(2))// && !ImGui::GetIO().KeyMods)
        {
            camera->pan(-ImGui::GetIO().MouseDelta.x / item_size.x, -ImGui::GetIO().MouseDelta.y / item_size.y);
            changed = true;
        }
    }

    if (ImGui::IsItemHovered()) {
        if (ImGui::GetIO().MouseWheel != 0 && !ImGui::GetIO().KeyMods) {
            const auto target_distance = camera->get_target_distance();
            camera->dolly(-ImGui::GetIO().MouseWheel * target_distance * 0.2);
            changed = true;
        }
    }
    return true;
}

void ImGui::BeginGLViewer(GLuint* fbo, GLuint* color_attachment, Camera* camera, const ImVec2& size_arg)
{
    ImGui::BeginGroup();

    // get item rect
    auto item_pos = ImGui::GetCursorPos(); // if child window has no border this is: 0,0
    auto item_size = CalcItemSize(size_arg, 540, 360);

    // add camera control widget
    ImGui::CameraControl(camera, { item_size });

    // add image widget with tecture
    ImGui::SetCursorPos(item_pos);
    ImGui::SetItemAllowOverlap();
    ImGui::Image((ImTextureID)*color_attachment, item_size, ImVec2(0, 1), ImVec2(1, 0));

    // draw border
    auto draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(ImGui::GetWindowPos() + item_pos, ImGui::GetWindowPos() + item_pos + item_size, ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]), ImGui::GetStyle().FrameRounding);

    // Resize texture to viewport size
    int tex_width, tex_height;
    { // get current texture size
        //ZoneScopedN("get viewer texture size");
        GLint current_tex;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_tex); // TODO: getting fbo is fairly harmful to performance.
        glBindTexture(GL_TEXTURE_2D, *color_attachment);

        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_height);
        glBindTexture(GL_TEXTURE_2D, current_tex);
    }

    if (tex_width != item_size.x || tex_height != item_size.y)
    { // update fbo size if necessary
        //ZoneScopedN("resize viewport fbo");
        //std::cout << "update viewport fbo: " << item_size.x << ", " << item_size.y << "\n";
        if (glIsFramebuffer(*fbo))
            glDeleteFramebuffers(1, fbo);
        if (glIsTexture(*color_attachment))
            glDeleteTextures(1, color_attachment);
        *color_attachment = imdraw::make_texture_float(item_size.x, item_size.y, NULL, GL_RGBA);
        *fbo = imdraw::make_fbo(*color_attachment);

        camera->aspect = (float)item_size.x / item_size.y;
    }

    // begin rendering to texture
    BeginRenderToTexture(*fbo, 0, 0, item_size.x, item_size.y);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}

void ImGui::EndGLViewer() {
    EndRenderToTexture();
    ImGui::EndGroup();
}