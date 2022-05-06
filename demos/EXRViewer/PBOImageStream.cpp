#include "PBOImageStream.h"
#include <iostream>
#include "OpenEXR/half.h"
#include "imgui.h"

PBOImageStream::PBOImageStream(int width, int height, int channels, int n)
{
    //ZoneScoped;
    pbos.resize(n);

    for (auto i = 0; i < n; i++)
    {
        GLuint pbo;
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * channels * sizeof(half), 0, GL_STREAM_DRAW);
        pbos[i].id = pbo;
        pbos[i].bbox = { 0,0, 0,0 };
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    display_index = 0;
    write_index = (display_index + 1) % pbos.size();

    m_width = width;
    m_height = height;
}

void PBOImageStream::onGUI()
{
    int buffer_size = pbos.size();
    if (ImGui::InputInt("buffer size", &buffer_size)) {
        reformat(m_width, m_height, 4, buffer_size);
    }

    for (auto i = 0; i < pbos.size(); i++)
    {

        if (write_index == i) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
        }
        if (display_index == i) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
        }
        ImGui::Text("%d", pbos[i].id); ImGui::SameLine();

        if (i == write_index || i == display_index) {
            ImGui::PopStyleColor();
        }
    }
    ImGui::NewLine();
}

void PBOImageStream::reformat(int width, int height, int channels, int n)
{
    m_width = width;
    m_height = height;
    for (auto i = 0; i < pbos.size(); i++) {
        glDeleteBuffers(1, &pbos[i].id);
    }

    pbos.resize(n);

    for (auto i = 0; i < n; i++)
    {
        GLuint pbo;
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * channels * sizeof(half), 0, GL_STREAM_DRAW);
        pbos[i].id = pbo;
        pbos[i].bbox = { 0,0, 0,0 };
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    display_index = 0;
    write_index = (display_index + 1) % pbos.size();
}

PBOImageStream::~PBOImageStream()
{
    for (auto i = 0; i < pbos.size(); i++) {
        glDeleteBuffers(1, &pbos[i].id);
    }
}

// display PBO bbox
std::tuple<int, int, int, int> PBOImageStream::bbox() const
{
    return pbos.at(display_index).bbox;
}

void PBOImageStream::write(void* pixels, const std::tuple<int, int, int, int>& bbox, const std::vector<std::string>& channels, unsigned long long typesize)
{
    display_index = (display_index + 1) % pbos.size();
    write_index = (display_index + 1) % pbos.size();

    auto& write_pbo = pbos[write_index];

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, write_pbo.id);
    void* ptr = (void*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr)
    {
        auto [x, y, w, h] = bbox;
        memcpy(ptr, pixels, w * h * channels.size() * typesize);
        write_pbo.bbox = bbox;
        write_pbo.channels = channels;
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    else {
        std::cerr << "cannot map PBO" << "\n";
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
}

void PBOImageStream::begin() { // BEGIN
    display_index = (display_index + 1) % pbos.size();
    write_index = (display_index + 1) % pbos.size();

    auto& write_pbo = pbos[write_index];

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, write_pbo.id);
    ptr = (void*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

    if (ptr) {
    }
    else {
        std::cerr << "cannot map PBO" << "\n";
    }

}

void PBOImageStream::end(std::tuple<int, int, int, int> bbox, std::vector<std::string> channels) {
    if (ptr)
    {
        auto& write_pbo = pbos[write_index];
        write_pbo.bbox = bbox;
        write_pbo.channels = channels;
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        ptr = NULL;
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
}