#include "ImGuiWidgets.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "glm/glm.hpp"
#include "glm/ext.hpp"

#include <iostream>
#include <format>

#include "IconsFontAwesome5.h"
void ImGui::Ranges(const std::vector<std::tuple<int, int>>& ranges, int v_min, int v_max, ImVec2 size) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;
    ImGuiContext& g = *GImGui;

    const ImVec2 item_pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    const float wrap_pos_x = window->DC.TextWrapPos;
    const bool wrap_enabled = (wrap_pos_x >= 0.0f);
   
    ImVec2 item_size = ImGui::CalcItemSize(size, 100, ImGui::GetFrameHeight()/2);
    ImRect bb(item_pos, item_pos +item_size );
    ItemSize(item_size);
    if (!ItemAdd(bb, 0))
        return;

    // Render
    ImGuiStyle& style = ImGui::GetStyle();
    auto dl = ImGui::GetWindowDrawList();
    // background
    
    dl->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImColor(style.Colors[ImGuiCol_FrameBg]), style.FrameRounding);

    // ticks
    ImVec2 grab_padding{ 2,2 };
    for (const auto& range : ranges) {
        
        auto [b, e] = range;
        auto p_min = ImGui::GetItemRectMin() + ImVec2((b-v_min) * (item_size.x- grab_padding.x*2+1) / (v_max - v_min+1)+ grab_padding.x, grab_padding.y);
        auto p_max = ImGui::GetItemRectMin() + ImVec2((e-v_min + 1) * (item_size.x - grab_padding.x * 2+1) / (v_max - v_min+1), item_size.y- grab_padding.y);
        dl->AddRectFilled(
            p_min,
            p_max,
            ImColor(style.Colors[ImGuiCol_PlotLines]),
            style.GrabRounding
        );
    }

    // border
    //dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImColor(1.0f, 1.0f, 1.0f, 1.0f));    
}

bool ImGui::Frameslider(const char* label, bool* is_playing, int* F, int v_min, int v_max, const std::vector<std::tuple<int, int>>& ranges, const char* format, ImGuiSliderFlags flags)
{
    bool changed = false;
    ImGui::BeginGroup();
    {
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_FAST_BACKWARD "##first frame")) {
            *F = v_min;
            changed = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_STEP_BACKWARD "##step backward")) {
            *F -= 1;
            if (*F < v_min) *F = v_max;
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
            *F += 1;
            if (*F > v_max) *F = v_min;
            changed = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_FAST_FORWARD "##last frame")) {
            *F = v_max;
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
        if (ImGui::SliderInt("##frame", F, v_min, v_max)) {
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

void ImGui::ImageViewer(const char* std_id, ImTextureID user_texture_id, int tex_width, int tex_height, ImVec2* pan, float* zoom, ImVec2 size, float zoom_speed, ImVec4 tint_col, ImVec4 border_col) {
    assert(("zoom is must be greater than 0", (*zoom) >0.0));
        
    auto itemsize = ImGui::CalcItemSize(size, 540 / 2, 360 / 2);

    // get item rect
    auto windowpos = ImGui::GetWindowPos();

    // Projection
    ImVec4 viewport(0, 0, itemsize.x, itemsize.y);

    auto camera_matrices = [&](ImVec2 pan, float zoom)->std::tuple<glm::mat4, glm::mat4> {
        //glm::mat4 projection = glm::ortho(0.0f,itemsize.x, 0.0f, itemsize.y, 0.1f, 10.0f);
        glm::mat4 projection = glm::ortho(-itemsize.x / 2, itemsize.x / 2, -itemsize.y / 2, itemsize.y / 2, 0.1f, 10.0f);
        glm::mat4 view(1);
        view = glm::translate(view, { pan.x, pan.y, 0.0f });
        view = glm::scale(view, { 1.0 / zoom, 1.0 / zoom, 1.0 / zoom });
        return { view, projection };
    };

    // world to screen
    auto project = [&](ImVec2 world)->ImVec2
    {
        auto [view, projection] = camera_matrices(*pan, *zoom);
        auto screen = glm::project(glm::vec3(world.x, world.y, 0.0f), glm::inverse(view), projection, glm::uvec4(viewport.x, viewport.y, viewport.z, viewport.w));
        return { screen.x, screen.y };
    };

    // screen to world
    auto unproject = [&](ImVec2 screen)->ImVec2
    {
        auto [view, projection] = camera_matrices(*pan, *zoom);
        auto world = glm::unProject(glm::vec3(screen.x, screen.y, 0.0f), glm::inverse(view), projection, glm::uvec4(viewport.x, viewport.y, viewport.z, viewport.w));
        return { world.x, world.y };
    };

    const auto set_zoom = [&](float value, ImVec2 pivot) {
        auto mouse_world = unproject(pivot);
        *zoom = value;
        *pan += mouse_world - unproject(pivot);
    };

    // GUI
    ImGui::PushID((ImGuiID)std_id);
    ImGui::BeginGroup();

    auto itempos = ImGui::GetCursorPos();
    auto mouse_item = ImGui::GetMousePos() - itempos - windowpos + ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
    auto mouse_item_prev = ImGui::GetMousePos() - ImGui::GetIO().MouseDelta - itempos - windowpos + ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
    ImGui::InvisibleButton("camera control", itemsize);
    ImGui::SetItemAllowOverlap();
    ImGui::SetItemUsingMouseWheel();
    if (ImGui::IsItemActive())
    {
        if (ImGui::IsMouseDragging(0) || ImGui::IsMouseDragging(2))// && !ImGui::GetIO().KeyMods)
        {
            ImVec2 offset = unproject(mouse_item_prev) - unproject(mouse_item);
            *pan += offset;
        }
    }

    if (ImGui::IsItemHovered()) {
        if (ImGui::GetIO().MouseWheel != 0 && !ImGui::GetIO().KeyMods)
        {
            float zoom_factor = 1.0 + (-ImGui::GetIO().MouseWheel * zoom_speed);
            set_zoom(*zoom / zoom_factor, mouse_item);
        }
    }
    ImGui::SetCursorPos(itempos);

    //auto bl = project(ImVec2(0,0));
    //auto tr = project(ImVec2(res[0], res[1]));
    auto bl = project(ImVec2(-tex_width / 2, -tex_height / 2));
    auto tr = project(ImVec2(tex_width / 2, tex_height / 2));

    /// Render to Texture
    //RenderTexture render_to_texture = RenderTexture(itemsize.x, itemsize.y);
    //render_to_texture.resize(itemsize.x, itemsize.y);
    //render_to_texture([&]() {
    //    imdraw::set_projection(glm::ortho(0.0f, itemsize.x, 0.0f, itemsize.y));
    //    imdraw::set_view(glm::mat4(1));
    //    imdraw::quad((GLuint)user_texture_id, { bl.x, bl.y }, { tr.x, tr.y });


    //    imdraw::disc({ mouse_item.x, mouse_item.y,0.0f}, 10);

    //});
    //ImGui::Image((ImTextureID)render_to_texture.color(), itemsize, {0,0}, {1,1}, tint_col, border_col);

    /// Frame with UV coods
    ImVec2 uv0 = (ImVec2(0, 0) - bl) / (tr - bl);
    ImVec2 uv1 = (itemsize - bl) / (tr - bl);
    ImGui::Image((ImTextureID)user_texture_id, itemsize, uv0, uv1, tint_col, { 0,0,0,0 });

    {// Image info
        ImGui::PushClipRect(itempos + windowpos, windowpos + itempos + itemsize, true);
        auto draw_list = ImGui::GetWindowDrawList();
        ImRect image_data_rect = ImRect(bl, tr);
        draw_list->AddRect(ImGui::GetWindowPos() + itempos + image_data_rect.Min, ImGui::GetWindowPos() + itempos + image_data_rect.Max, ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]), 0.0);

        std::string str;
        //ImGui::SetCursorPos(itempos + image_data_rect.Max);
        if (tex_width == 1920 && tex_height == 1080)
        {
            str = "HD";
        }
        else if (tex_width == 3840 && tex_height == 2160)
        {
            str = "UHD 4K";
        }
        else if (tex_width == tex_height)
        {
            str = "Square %d";
        }
        else
        {
            str = std::format("{}x{}", tex_width, tex_height);
        }

        draw_list->AddText(windowpos + itempos + image_data_rect.Max, ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), str.c_str());

        draw_list->AddRect(windowpos + itempos, windowpos + itempos + itemsize, ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]));
        ImGui::PopClipRect();
    }

    /// Toolbar
    {
        ImGui::SetCursorPos(itempos);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);

        ImGui::SetNextItemWidth(ImGui::CalcTextSize("100%").x + ImGui::GetStyle().FramePadding.x * 2);
        int zoom_percent = *zoom * 100;
        if (ImGui::DragInt("##zoom", &zoom_percent, 0.1, 1, 100, "%d%%", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_Vertical)) {
            set_zoom(zoom_percent / 100.0, itemsize / 2);
        }

        if (ImGui::IsItemClicked(1)) {
            *zoom = 1.0;
        }
        ImGui::SameLine();
        ImGui::Button("pan", { 0,0 });
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            std::cout << "item active" << "\n";
            ImVec2 offset = unproject(mouse_item_prev) - unproject(mouse_item);
            *pan += offset;
        }
        if (ImGui::IsItemClicked(1))
        {
            *pan = ImVec2(0, 0);
        }
        ImGui::SameLine();
        ImGui::SameLine();
        if (ImGui::Button("fit")) {
            *zoom = std::min(itemsize.x / tex_width, itemsize.y / tex_height);
            *pan = ImVec2(0, 0);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    ImGui::EndGroup();
    ImGui::PopID();


}

bool ImGui::SliderInt(const char* label, Nodes::Attribute<int>* attr, int v_min, int v_max)
{
    auto val = attr->get();
    if (ImGui::SliderInt(label, &val, v_min, v_max)) {
        attr->set(val);
        return true;
    }
    return false;
}

bool ImGui::SliderFloat(const char* label, Nodes::Attribute<float>* attr, int v_min, int v_max)
{
    auto val = attr->get();
    if (ImGui::SliderFloat(label, &val, v_min, v_max)) {
        attr->set(val);
        return true;
    }
    return false;
}