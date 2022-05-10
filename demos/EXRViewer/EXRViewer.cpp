// EXRViewer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <vector>
#include <array>
#include <ranges>

#include <format>
#include <iostream>
#include <string>
#include <string_view>

#include <numeric>
#include <charconv> // std::to_chars
#include <iostream>
//#include "../../tracy/Tracy.hpp"

// ImGui
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h" // use imgui math operators

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"
#include "glhelpers.h"

// OpenEXR
#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfChannelList.h>

// FileSequence
#include "FileSequence.h"

// sequence readers
#include "Readers/EXRSequenceReader.h"
#include "Readers/EXRLayerManager.h"
#include "Readers/OIIOSequenceReader.h"

#include "PixelsRenderer.h"
#include "RenderPlates/RenderPlate.h"
#include "RenderPlates/CorrectionPlate.h"
#include "PBOImageStream.h"
#include "ImGuiWidgets.h"

#include "helpers.h"

bool is_playing = false;
int F, first_frame, last_frame;

int selected_viewport_background{ 1 };//0: transparent 1: checkerboard 2:black

FileSequence sequence;
std::unique_ptr<OIIOSequenceReader>reader;

bool use_pbostream{false};
std::unique_ptr<PBOImageStream> pbostream;
std::unique_ptr<EXRLayerManager> layer_manager;
std::unique_ptr<PixelsRenderer> renderer;

ImGui::GLViewerState viewer_state;

std::unique_ptr<CorrectionPlate> correction_plate;
std::unique_ptr<RenderPlate> polka_plate;
std::unique_ptr<RenderPlate> checker_plate;
std::unique_ptr<RenderPlate> black_plate;
bool READ_DIRECTLY_TO_PBO{ true };

bool sidebar_visible{true};

void open(std::filesystem::path filename)
{
    std::cout << "opening: " << filename << "\n";
    if (!std::filesystem::exists(filename)) {
        std::cout << "file does not exist! " << filename << "\n";
    }
    sequence = FileSequence(filename);
    first_frame = sequence.first_frame;
    last_frame = sequence.last_frame;
    F = sequence.first_frame;
    F = sequence.first_frame;

    reader = std::make_unique<OIIOSequenceReader>(sequence);
    auto [display_width, display_height] = reader->size();
    pbostream = std::make_unique<PBOImageStream>(display_width, display_height, 4, 3);
    renderer = std::make_unique<PixelsRenderer>(display_width, display_height);
    layer_manager = std::make_unique<EXRLayerManager>(sequence.item(sequence.first_frame));
    correction_plate = std::make_unique<CorrectionPlate>(renderer->width, renderer->height, renderer->color_attachment);

    viewer_state.camera.fit(renderer->width, renderer->height);
}

void drop_callback(GLFWwindow* window, int argc, const char** argv)
{
    for (auto i = 0; i < argc; i++)
    {
        std::cout << "drop file: " << argv[i] << "\n";
    }
    if (argc > 0) {
        open(argv[0]);
    }
}

void update()
{
    auto [display_width, display_height] = reader->size();

    if (use_pbostream)
    {
        if (!READ_DIRECTLY_TO_PBO)
        {
            // read to memory
            static void* pixels = malloc((size_t)display_width * display_height * 4 * sizeof(half));
            if (pixels == NULL) std::cerr << "NULL allocation" << "\n";
            reader->read_to_memory(pixels);

            // memory to pbo
            pbostream->write(pixels, reader->bbox(), reader->selected_channels(), sizeof(half));

            // pbo to texture
            auto& pbo = pbostream->pbos[pbostream->display_index];
            renderer->update_from_pbo(pbo.id, pbo.bbox, pbo.channels, GL_HALF_FLOAT);
        }
        else {

            // read to pbo
            pbostream->begin();
            if (pbostream->ptr)
            {
                reader->read_to_memory(pbostream->ptr);
            }

            pbostream->end(reader->bbox(), reader->selected_channels());

            // pbo to texture
            auto& display_pbo = pbostream->pbos[pbostream->display_index];
            renderer->update_from_pbo(display_pbo.id, display_pbo.bbox, display_pbo.channels, GL_HALF_FLOAT);
        }
    }
    else
    {
        static void* pixels = malloc((size_t)display_width * display_height * 4 * sizeof(half));
        if (pixels == NULL) std::cerr << "NULL allocation" << "\n";
        reader->read_to_memory(pixels);
        
        renderer->update_from_data(pixels, reader->bbox(), reader->selected_channels(), GL_HALF_FLOAT);
    }


    correction_plate->set_input_tex(renderer->color_attachment);
    correction_plate->update();
}

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    for (auto i = 0; i < argc; i++) {
        std::cout << i << ": " << argv[i] << "\n";
    }

    // Fix Current Working Directory
    if (argc > 0) {
        try {

            fs::current_path(fs::path(argv[0]).parent_path()); // set working directory to exe
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
    std::cout << "current working directory: " << fs::current_path() << "\n";
    
    // setup glazy
    glazy::init();
    glazy::set_vsync(false);
    glfwSetDropCallback(glazy::window, drop_callback);

    // Open file
    if (argc == 2)
    {
        // open dropped file
        open(argv[1]);
    }
    else
    {
        // open default sequence
        open("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");
    }

    /// Setup render plates
    polka_plate = std::make_unique<RenderPlate>("PASS_THROUGH_CAMERA.vert", "polka.frag");
    checker_plate = std::make_unique<RenderPlate>("checker.vert", "checker.frag");
    black_plate = std::make_unique<RenderPlate>("checker.vert","constant.frag");

    /// Run main loop
    while (glazy::is_running())
    {
        glazy::new_frame();

        try {

            if (is_playing)
            {
                F++;
                if (F > last_frame) F = first_frame;
                reader->set_current_frame(F);
            }

            if (ImGui::IsKeyPressed('F')) {
                viewer_state.camera.fit(renderer->width, renderer->height);
            }

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open", "")) {
                        auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg");
                        open(filepath);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View")) {
                    if (ImGui::MenuItem("fit", "f"))
                    {
                        viewer_state.camera.fit(renderer->width, renderer->height);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Windows")) {
                    ImGui::MenuItem("sidebar", "", &sidebar_visible);
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            if (ImGui::Begin("Viewer", (bool*)0, ImGuiWindowFlags_MenuBar))
            {
                if (ImGui::BeginMenuBar())
                {
                    const auto& layer_names = layer_manager->layer_names();
                    ImGui::SetNextItemWidth(ImGui::CalcComboWidth(layer_names));
                    if (ImGui::BeginCombo("##layers", layer_manager->current_name().c_str()))\
                    {
                        for (auto i = 0; i < layer_names.size(); i++) {
                            ImGui::PushID(i);
                            bool is_selected = i == layer_manager->current();
                            if (ImGui::Selectable(layer_names.at(i).c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                                layer_manager->set_current(i);
                                reader->set_selected_part_idx(layer_manager->current_part_idx());
                                reader->set_selected_channels(layer_manager->current_channel_ids());
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("layers");

                    ImGui::Separator();

                    ImGui::BeginGroup();
                    {
                        if (ImGui::Button("fit"))
                        {
                            viewer_state.camera.fit(renderer->width, renderer->height);
                        }
                        if (ImGui::Button("flip y")) {
                            viewer_state.camera = Camera(
                                viewer_state.camera.eye * glm::vec3(1, 1, -1),
                                viewer_state.camera.target * glm::vec3(1, 1, -1),
                                viewer_state.camera.up * glm::vec3(1, -1, 1),
                                viewer_state.camera.ortho,
                                viewer_state.camera.aspect,
                                viewer_state.camera.fovy,
                                viewer_state.camera.tiltshift,
                                viewer_state.camera.near_plane,
                                viewer_state.camera.far_plane
                            );
                        }
                        //ImGui::MenuItem(ICON_FA_CHESS_BOARD, "", &display_checkerboard);
                        ImGui::SetNextItemWidth(ImGui::GetFrameHeightWithSpacing());
                        std::vector<std::string> preview_texts{ ICON_FA_SQUARE_FULL, ICON_FA_CHESS_BOARD ," " };
                        if (selected_viewport_background == 0) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                        }
                        else {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                        }
                        if (ImGui::BeginCombo("##background", preview_texts[selected_viewport_background].c_str(), ImGuiComboFlags_NoArrowButton))
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                            if (ImGui::Selectable(ICON_FA_SQUARE_FULL, selected_viewport_background == 0)) {
                                selected_viewport_background = 0;
                            }
                            ImGui::PopStyleColor();

                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                            if (ImGui::Selectable(ICON_FA_CHESS_BOARD, selected_viewport_background == 1)) {
                                selected_viewport_background = 1;
                            };

                            if (ImGui::Selectable(" ", selected_viewport_background == 2)) {
                                selected_viewport_background = 2;
                            };
                            ImGui::PopStyleColor();
                            ImGui::EndCombo();
                        }
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("background");
                        }
                    }
                    ImGui::EndGroup();

                    ImGui::Separator();
                    correction_plate->onGui();

                    ImGui::EndMenuBar();
                }

                ImGui::BeginGLViewer(&viewer_state.viewport_fbo, &viewer_state.viewport_color_attachment, &viewer_state.camera, { -1,-1 });
                {
                    glClearColor(0, 0, 0, 0);
                    glClear(GL_COLOR_BUFFER_BIT);
                    imdraw::set_projection(viewer_state.camera.getProjection());
                    imdraw::set_view(viewer_state.camera.getView());
                    glm::mat4 M{ 1 };
                    M = glm::scale(M, { renderer->width / 2, renderer->height / 2, 1 });
                    M = glm::translate(M, { 1,1, 0 });

                    if (selected_viewport_background == 1)
                    {
                        checker_plate->set_uniforms({
                            {"projection", viewer_state.camera.getProjection()},
                            {"view", viewer_state.camera.getView()},
                            {"model", M},
                            {"tile_size", 8}
                            });
                        checker_plate->render();
                    }

                    if (selected_viewport_background == 0)
                    {
                        black_plate->set_uniforms({
                            {"projection", viewer_state.camera.getProjection()},
                            {"view", viewer_state.camera.getView()},
                            {"model", M},
                            {"uColor", glm::vec3(0,0,0)}
                            });
                        black_plate->render();
                    }

                    //polka_plate->set_uniforms({
                    //    {"projection", viewer_state.camera.getProjection()},
                    //    {"view", viewer_state.camera.getView()},
                    //    {"model", glm::mat4(1)},
                    //    {"radius", 0.01f}
                    //});
                    //polka_plate->render();
                    correction_plate->render();

                    // display spec
                    
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.5,.5,.5, 0.5));
                        auto width = renderer->width;
                        auto height = renderer->height;
                        
                        auto item_pos = ImGui::GetItemRectMin() - ImGui::GetWindowPos(); // window space
                        auto item_size = ImGui::GetItemRectSize();
                        auto store_cursor = ImGui::GetCursorPos();

                        {
                            
                            glm::vec2 pos = { width, height };

                            glm::vec3 screen_pos = glm::project(glm::vec3(pos, 0.0), viewer_state.camera.getView(), viewer_state.camera.getProjection(), glm::vec4(0, 0, item_size.x, item_size.y));
                            screen_pos.y = item_size.y - screen_pos.y; // invert y

                            ImGui::SetCursorPos({ screen_pos.x + item_pos.x, screen_pos.y + item_pos.y });
                            if (width == 1920 && height == 1080)
                            {
                                ImGui::Text("HD");
                            }
                            else if (width == 3840 && height == 2160)
                            {
                                ImGui::Text("UHD 4K");
                            }
                            else if (width == height)
                            {
                                ImGui::Text("Square %d", width);
                            }
                            else
                            {
                                ImGui::Text("%dx%d", width, height);
                            }
                        }
                        {
                            auto [x, y, w, h] = renderer->m_bbox;
                            glm::vec2 pos = { x, height-y };

                            glm::vec3 screen_pos = glm::project(glm::vec3(pos, 0.0), viewer_state.camera.getView(), viewer_state.camera.getProjection(), glm::vec4(0, 0, item_size.x, item_size.y));
                            screen_pos.y = item_size.y - screen_pos.y; // invert y

                            ImGui::SetCursorPos({ screen_pos.x + item_pos.x, screen_pos.y + item_pos.y });
                            ImGui::Text("%dx%d", x, y);
                        }

                        {
                            auto [x, y, w, h] = renderer->m_bbox;
                            glm::vec2 pos = { x+w, height- (y+h) };
                            
                            glm::vec3 screen_pos = glm::project(glm::vec3(pos, 0.0), viewer_state.camera.getView(), viewer_state.camera.getProjection(), glm::vec4(0, 0, item_size.x, item_size.y));
                            screen_pos.y = item_size.y - screen_pos.y; // invert y

                            ImGui::SetCursorPos({ screen_pos.x + item_pos.x, screen_pos.y + item_pos.y });
                            ImGui::Text("%dx%d", x, y);
                        }
                        ImGui::SetCursorPos(store_cursor); //restore cursor
                        ImGui::PopStyleColor();
                    }                }
                ImGui::EndGLViewer();
            }
            ImGui::End();

            {
                auto viewsize = ImGui::GetMainViewport()->WorkSize;
                ImGui::SetNextWindowSize({ 300, viewsize.y * 2 / 3 });
                ImGui::SetNextWindowPos({ viewsize.x - 320, viewsize.y * 1 / 3 / 2 });

                if (sidebar_visible)
                {
                    if (ImGui::Begin("##Sidebar", &sidebar_visible, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
                    {
                        if (ImGui::BeginTabBar("Sidebar-tabs"))
                        {
                            if (ImGui::BeginTabItem("Info (current file)"))
                            {
                                auto file = Imf::MultiPartInputFile(sequence.item(reader->current_frame()).string().c_str());
                                ImGui::TextWrapped(get_infostring(file).c_str());
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Channels"))
                            {
                                auto file = Imf::MultiPartInputFile(sequence.item(sequence.first_frame).string().c_str());
                                auto parts = file.parts();
                                for (auto p = 0; p < parts; p++)
                                {
                                    ImGui::PushID(p);
                                    auto in = Imf::InputPart(file, p);
                                    auto header = in.header();
                                    auto cl = header.channels();
                                    ImGui::LabelText("Part", "%d", p);
                                    ImGui::LabelText("Name", "%s", header.hasName() ? header.name().c_str() : "");
                                    ImGui::LabelText("View", "%s", header.hasView() ? header.view().c_str() : "");
                                    if (ImGui::BeginTable("channels table", 4, ImGuiTableFlags_NoBordersInBody))
                                    {
                                        ImGui::TableSetupColumn("name");
                                        ImGui::TableSetupColumn("type");
                                        ImGui::TableSetupColumn("sampling");
                                        ImGui::TableSetupColumn("linear");
                                        ImGui::TableHeadersRow();
                                        auto channels =reader->selected_channels();
                                        for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
                                        {
                                            //ImGui::TableNextRow();
                                            ImGui::TableNextColumn();
                                            bool is_selected = (reader->selected_part_idx() == p) && std::find(channels.begin(), channels.end(), std::string(i.name())) != channels.end();
                                            if (ImGui::Selectable(i.name(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                                            {
                                                if (ImGui::GetIO().KeyCtrl && reader->selected_part_idx() == p)
                                                {
                                                    auto channel_name = std::string(i.name());
                                                    auto current_selected_channels = reader->selected_channels();
                                                    current_selected_channels.push_back(channel_name);
                                                    reader->set_selected_channels(current_selected_channels);
                                                }
                                                else
                                                {
                                                    reader->set_selected_part_idx(p);
                                                    auto channel_name = std::string(i.name());
                                                    reader->set_selected_channels({ channel_name });
                                                }
                                            }

                                            ImGui::TableNextColumn();
                                            ImGui::Text("%s", to_string(i.channel().type).c_str());

                                            ImGui::TableNextColumn();
                                            ImGui::Text("%d %d", i.channel().xSampling, i.channel().ySampling);

                                            ImGui::TableNextColumn();
                                            ImGui::Text("%s", i.channel().pLinear ? "true" : "false");
                                        }
                                        ImGui::EndTable();
                                    }
                                    ImGui::PopID();
                                }

                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Inspector"))
                            {
                                if (ImGui::CollapsingHeader("FileSequence", ImGuiTreeNodeFlags_DefaultOpen))
                                {
                                    ImGui::LabelText("pattern", "%s", sequence.pattern.string().c_str());
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::SetTooltip("%s", sequence.pattern.string().c_str());
                                    }
                                    ImGui::LabelText("range", "%d-%d", sequence.first_frame, sequence.last_frame);

                                    const auto& missing_frames = sequence.missing_frames();
                                    if (!missing_frames.empty())
                                    {
                                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0,1));
                                        std::string missing_frames_string;
                                        for (auto F : sequence.missing_frames()) {
                                            missing_frames_string += std::to_string(F) + ", ";
                                        }
                                        ImGui::LabelText("missing frames", "%s", missing_frames_string.c_str());
                                        ImGui::PopStyleColor();

                                    }
                                }
                                if (ImGui::CollapsingHeader("LayerManager", ImGuiTreeNodeFlags_DefaultOpen))
                                {
                                    if (layer_manager->onGUI()) {
                                        std::cout << "layer manager changed" << "\n";
                                    }
                                }

                                if (ImGui::CollapsingHeader("Reader", ImGuiTreeNodeFlags_DefaultOpen)) {
                                    reader->onGUI();
                                }
                                if (ImGui::CollapsingHeader("PBO Stream", ImGuiTreeNodeFlags_DefaultOpen))
                                {
                                    ImGui::Checkbox("use pbostream", &use_pbostream);
                                    if (use_pbostream)
                                    {
                                        ImGui::Checkbox("READ DIRECTLY TO PBO", &READ_DIRECTLY_TO_PBO);
                                        pbostream->onGUI();
                                    }
                                }
                                if (ImGui::CollapsingHeader("PixelsRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                                    renderer->onGUI();
                                }
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                    }
                    ImGui::End();
                }
            }

            if(sequence.length()>1)
            {
                auto viewsize = ImGui::GetMainViewport()->WorkSize;
                ImGui::SetNextWindowSize({ viewsize.x * 2 / 3,0 });
                ImGui::SetNextWindowPos({ viewsize.x * 1 / 3 / 2, viewsize.y - 80 });
                if (ImGui::Begin("Timeline", (bool*)0, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration))
                {
                    if (ImGui::Frameslider("timeslider", &is_playing, &F, first_frame, last_frame)) {
                        reader->set_current_frame(F);
                    }
                }
                ImGui::End();
            }
            update();
            glazy::end_frame();
            //FrameMark;
        }

        catch (const std::exception& ex) {
            std::cout << ex.what() << "\n";
            std::cin.get();
            // ...
        }
        catch (const std::string& ex) {
            std::cout << ex << "\n";
            std::cin.get();
        }
        catch (...) {
            std::cin.get();
        }
    }
    glazy::destroy();
}