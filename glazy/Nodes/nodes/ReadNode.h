#pragma once

#include <string>
#include <tuple>
#include <vector>
#include <map>
#include "../MovieIO/MovieIO.h"
#include "nodes.h"
#include "imgui.h"

class ReadNode
{
private:
    std::string _cache_pattern;

    using MemoryImage = std::tuple<void*, int, int, std::vector<std::string>, OIIO::TypeDesc>; //ptr, width, height, channels, format

    int _first_frame;
    int _last_frame;

    std::unique_ptr<MovieIO::MovieInput> _current_file;

public:
    Nodes::Attribute<std::string> file; // filename or sequence pattern
    Nodes::Attribute<int> frame;
    Nodes::Outlet<std::tuple<void*, TextureSpec>> plate_out;

    Nodes::Attribute<int> selected_channelset_idx;

    void* memory;
    size_t capacity{ 0 };

    std::map<int, MemoryImage> _cache; // memory, width, height, channels

    int first_frame() const {
        return _first_frame;
    }

    int last_frame() const {
        return _last_frame;
    }

    int length() const {
        return _last_frame - _first_frame + 1;
    }

    ReadNode()
    {
        // alloc default memory with a small imagesize
        capacity = 256 * 256 * 4 * sizeof(half);
        memory = std::malloc(capacity);

        // setup inlet triggers
        file.onChange([&](std::string pattern) {
            _cache.clear();
            try
            {
                _current_file = MovieIO::MovieInput::open(pattern);

                auto [b, e] = _current_file->range();
                _first_frame = b;
                _last_frame = e;
                frame.set(_first_frame);
            }
            catch (std::filesystem::filesystem_error err)
            {
                _first_frame = 0;
                _last_frame = 0;
                _current_file.reset();
                return;
            }
            });

        frame.onChange([&](int F) {
            read();
            });
    }

    ~ReadNode()
    {
        free(memory);
    }

    void read()
    {
        if (!_current_file) return;
        if (!_cache.contains(frame.get()))
        {
            ZoneScoped;
            if (!_current_file->seek(frame.get())) {
                return;
            };

            auto info = _current_file->info();

            auto required_capacity = info.width * info.height * info.channels.size() * info.format.size();
            if (capacity < required_capacity)
            {
                std::cout << "reader->realloc" << "\n";
                if (void* new_memory = std::realloc(memory, required_capacity))
                {
                    memory = new_memory;
                    capacity = required_capacity;
                }
                else {
                    throw std::bad_alloc();
                }
            }

            _current_file->read(memory);
            if (memory == nullptr) {
                return;
            }
            auto& img_cache = _cache[frame.get()];
            std::get<0>(img_cache) = malloc(capacity);
            std::memcpy(std::get<0>(img_cache), memory, capacity);
            std::get<1>(img_cache) = info.width;
            std::get<2>(img_cache) = info.height;
            std::get<3>(img_cache) = info.channels;
            std::get<4>(img_cache) = info.format;
        }

        auto [memory, w, h, channels, f] = _cache.at(frame.get());
        GLenum glformat;

        if (channels == std::vector<std::string>({ "B", "G", "R", "A" })) {
            glformat = GL_BGRA;
        }
        else if (channels == std::vector<std::string>({ "B", "G", "R" }))
        {
            glformat = GL_BGR;
        }
        else if (channels.size() == 4) {
            glformat = GL_RGBA;
        }
        else if (channels.size() == 3) {
            glformat = GL_RGB;
        }

        GLenum gltype;
        if (f == OIIO::TypeHalf) {
            gltype = GL_HALF_FLOAT;
        }
        else if (f == OIIO::TypeUInt8)
        {
            gltype = GL_UNSIGNED_BYTE;
        }
        else {
            throw "format is not recognized";
        }
        plate_out.trigger({ memory, TextureSpec(0, 0, w, h, glformat, gltype) });
    }

    std::vector<std::tuple<int, int>> cached_range() const {
        // collect cache frame ranges
        std::vector<int> cached_frames;
        for (const auto& [F, img] : _cache)
        {
            cached_frames.push_back(F);
        }
        std::sort(cached_frames.begin(), cached_frames.end());

        std::vector<std::tuple<int, int>> ranges;

        for (auto F : cached_frames) {
            if (!ranges.empty()) {
                auto& last_range = ranges.back();
                auto [begin, end] = last_range;
                if (F == end + 1) {
                    std::get<1>(last_range)++;
                }
                else {
                    ranges.push_back({ F, F });
                }
            }
            else {
                ranges.push_back({ F, F });
            }
        }

        return ranges;
    }

    void onGUI()
    {

        // File ----------------------------------------------------
        ImGui::Text("File");

        std::string filename = file.get();
        if (ImGui::InputText("file", &filename)) {
            file.set(filename);
        }
        ImGui::SameLine();

        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::GetTextLineHeightWithSpacing());
        ImGui::SetNextItemWidth(ImGui::GetTextLineHeightWithSpacing());
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##open")) {
            auto selected_file = glazy::open_file_dialog("all supported\0*.mp4;*.exr;*.jpg\0EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg\0MP4 movies\0*.mp4");
            if (!selected_file.empty())
            {
                file.set(selected_file.string());
            }
        }
        ImGui::LabelText("range", "%d-%d", _first_frame, _last_frame);
        ImGui::SliderInt("frame", &frame, _first_frame, _last_frame);

        ImGui::Separator(); // ---------------------------------------

        // Cache -----------------------------------------------------
        ImGui::Text("Cache");
        int used_memory = 0;
        for (const auto& [key, memory_img] : _cache)
        {
            const auto& [mem, w, h, channels, f] = memory_img;


            used_memory += w * h * channels.size() * f.size();
        }
        ImGui::LabelText("images", "%d", _cache.size());
        ImGui::LabelText("memory", "%.2f MB", used_memory / pow(1000, 2));

        for (const auto& inlet : plate_out.targets())
        {
            ImGui::Text("inlet: %s", "target");
        }

        ImGui::Ranges(cached_range(), _first_frame, _last_frame);

        ImGui::Separator(); // ---------------------------------------

        // ChannelSets
        ImGui::Text("ChannelSets");
        if (_current_file)
        {
            for (auto channelset : _current_file->channel_sets())
            {
                std::string channelset_label = channelset.name.empty() ? "-" : channelset.name;
                std::string channels_label = join_string(channelset.channels, ", ");
                ImGui::Text("%s", channelset_label.c_str());
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.66f));
                ImGui::Text("%s", channels_label.c_str());
                ImGui::PopStyleColor();
            }
        }

        ImGui::Separator(); // ---------------------------------------

        // Info -----------------------------------------------------
        ImGui::Text("Info");
        if (_current_file)
        {
            auto info = _current_file->info();
            ImGui::LabelText("size", "%dx%d", info.width, info.height);

            ImGui::LabelText("channels", "%s", join_string(info.channels, ", ").c_str());

            ImGui::LabelText("format", "%s", info.format.c_str());
        }
    }
};

