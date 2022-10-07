// Nodes.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include <vector>
#include <functional>
#include <any>


// ImGui
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h> // use imgui math operators

#include "glazy.h"

#include <filesystem>
#include <tuple>

#include <OpenImageIO/imageio.h>

#include <glad/glad.h>

//#include <../../tracy/Tracy.hpp>
#include "Camera.h"


#include "ImGuiWidgets.h"

#include <future>

#include "pathutils.h"

#include <opencv2/opencv.hpp>

#include "glhelpers.h"
#include "stringutils.h"

class RenderTexture
{
private:
    int mWidth, mHeight;
    GLuint colorattachment;
    GLuint fbo;
public:
    RenderTexture(int w, int h):mWidth(w), mHeight(h) {
        colorattachment = imdraw::make_texture(w, h, NULL, GL_RGBA);
        fbo = imdraw::make_fbo(colorattachment);
    }

    void resize(int w, int h)
    {

        if (w == mWidth && h == mHeight) return;

        if (glIsTexture(colorattachment)) {
            glDeleteTextures(1, &colorattachment);
        }

        colorattachment = imdraw::make_texture(w, h, NULL, GL_RGBA);
        fbo = imdraw::make_fbo(colorattachment);

        mWidth = w;
        mHeight = h;
    }

    void begin() const
    {
        // setup fbo
        ImVec4 viewport(0.0f, 0.0f, width(), height()); // x, y, w, h
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(viewport.x, viewport.y, viewport.z, viewport.w);

        // clear canvas    
        glClearColor(0, 0, 0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // draw to fbo
        // ...
    }

    void end() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GLuint color() {
        return colorattachment;
    }

    int width() const
    {
        return mWidth;
    }

    int height() const
    {
        return mHeight;
    }

    std::tuple<int,int> size() {
        return { mWidth, mHeight };
    }

    void operator()(std::function<void()> cb)
    {
        begin();
        cb();
        end();
    }

    void operator()(int w, int h, std::function<void()> cb)
    {
        resize(w, h);
        begin();
        cb();
        end();
    }
};

#include "nodes/nodes.h"
#include "MovieIO/MovieIO.h"
#include "nodes/ReadNode.h"
#include "nodes/ViewportNode.h"

#include "MovieIO/MovieIO.h"
#include "nodes/MemoryImage.h"

#include "OpenImageIO/imageio.h"


int main(int argc, char* argv[])
{

    /// Open with file argument
    // Fix Current Working Directory
    if (argc > 0) {
        try {

            std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path()); // set working directory to exe
        }
        catch (std::filesystem::filesystem_error const& ex) {
            std::cout
                << "what():  " << ex.what() << '\n'
                << "path1(): " << ex.path1() << '\n'
                << "path2(): " << ex.path2() << '\n'
                << "code().value():    " << ex.code().value() << '\n'
                << "code().message():  " << ex.code().message() << '\n'
                << "code().category(): " << ex.code().category().name() << '\n';
        }
    }
    std::cout << "current working directory: " << std::filesystem::current_path() << "\n";

    glazy::init({.docking=true, .viewports=false});


    auto& style = ImGui::GetStyle();
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;
    style.PopupBorderSize = 0;
    style.FrameBorderSize = 0;
    glazy::set_vsync(false);

    // create state
    bool IS_PLAYING = false;

    // create graph
    auto read_node = ReadNode();
    auto viewport_node = ViewportNode();
    read_node.plate_out.target(viewport_node.image_in);

    // Open file
    if (argc == 2)
    {
        // open dropped file
        read_node.file.set(argv[1]);
    }
    else
    {
        // open default sequence

        //read_node.file.set("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0004.exr");
        //read_node.file.set("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.%04d.exr");
        //read_node.file.set("C:/Users/andris/Desktop/testimages/58_31_FIRE_v20.mp4");

        read_node.file.set("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.%04d.exr");
    }
    
    //viewport_node.fit();

    while (glazy::is_running()) {
        glazy::new_frame();
        //FrameMark;

        ImVec2 itemsize;
        static ImVec2 pan{ 0,0 };
        static float zoom{ 1 };

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open", ""))
                {
                    read_node.file.set("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("100%", "")) {
                    zoom = 1.0;
                }

                if (ImGui::MenuItem("Center", "")) {
                    pan = ImVec2(0, 0);
                }

                if (ImGui::MenuItem("Fit", "")) {
                    zoom = std::min(itemsize.x / viewport_node._width, itemsize.y / viewport_node._height);
                    pan = ImVec2(0, 0);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (ImGui::Begin("Viewer##Panel")) {

            ImVec2 cursor_pos = ImGui::GetCursorPos();
            ImGui::ImageViewer("viewer", (ImTextureID)viewport_node._correctedtex, viewport_node._width, viewport_node._height, &pan, &zoom, { -1,-1 });

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
            int selected_device = (int)viewport_node.selected_device.get();
            int combo_width = ImGui::CalcComboWidth({ "linear", "sRGB", "rec709" });
            ImGui::SetCursorPos(cursor_pos+ImVec2(180,0));
            ImGui::SetNextItemWidth(combo_width);
            if (ImGui::Combo("##device", &selected_device, { "linear", "sRGB", "rec709" }))
            {
                viewport_node.selected_device.set((ViewportNode::DeviceTransform)selected_device);
            }
            itemsize = ImGui::GetItemRectSize();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        ImGui::End();

        auto viewsize = ImGui::GetMainViewport()->WorkSize;
        
        if (read_node.length() > 1)
        {
            ImGui::SetNextWindowSize({ viewsize.x * 2 / 3,0 });
            ImGui::SetNextWindowPos({ viewsize.x * 1 / 3 / 2, viewsize.y - 80 });
            if (ImGui::Begin("Frameslider##Panel"))
            {
                int F = read_node.frame.get();
                if (ImGui::Frameslider("frameslider", &IS_PLAYING, &F, read_node.first_frame(), read_node.last_frame(), read_node.cached_range()))
                {
                    read_node.frame.set(F);
                }


            }
            ImGui::End();
        }

        ImGui::SetNextWindowSize({ 300, viewsize.y * 2 / 3 });
        ImGui::SetNextWindowPos({ viewsize.x-300, viewsize.y * 1 / 3 / 2 });
        if (ImGui::Begin("Inspector##Panel")) {
            if (ImGui::CollapsingHeader("read", ImGuiTreeNodeFlags_DefaultOpen))
            {
                read_node.onGUI();
            }
            if (ImGui::CollapsingHeader("viewport", ImGuiTreeNodeFlags_DefaultOpen)) {
                viewport_node.onGUI();
            }
        }
        ImGui::End();

        if (IS_PLAYING)
        {
            int frame = read_node.frame.get();
            
            frame+=1;
            if (read_node.last_frame() < frame) {
                frame = read_node.first_frame();
            }
            read_node.frame.set(frame);
        }

        glazy::end_frame();

        //FrameMark;
    }
    glazy::destroy();

}
