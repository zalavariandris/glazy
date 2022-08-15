#include "ReadNode.h"

#include <Windows.h>
#include "imgui.h"
#include <imgui_stdlib.h>
#include "../ImGuiWidgets.h"

#include <filesystem>
#include <IconsFontAwesome5.h>
#include "glazy.h"

ReadNode::ReadNode()
{
    // setup inlet triggers
    file.onChange([&](std::string pattern) {
        _cache.clear();
        try
        {
            _movie_input = MovieIO::MovieInput::open(pattern);

            auto [b, e] = _movie_input->range();
            _first_frame = b;
            _last_frame = e;
            frame.set(_first_frame);
        }
        catch (std::filesystem::filesystem_error err)
        {
            _first_frame = 0;
            _last_frame = 0;
            _movie_input.reset();
            return;
        }
        });

    frame.onChange([&](int F) {
        read();
        });

    selected_channelset_idx.onChange([&](int val) {
        std::cout << "selected channel set changed: " << val << "\n";
        read();
        });

    enable_cache.onChange([&](bool val) {
        _cache.clear();
        });
}

void ReadNode::read()
{
    if (!_movie_input) return;

    std::shared_ptr<MemoryImage> image;

    if (!enable_cache.get() || !_cache.contains(CacheKey(frame.get())))
    {
        //ZoneScoped;

        // get info at current frame
        if (!_movie_input->seek(frame.get())) {
            return;
        };

        MovieIO::Info info = _movie_input->info();

        // Read pixels
        // TODO: REALLOC is faster. Implement it on MemoryImage
        image = std::make_shared<MemoryImage>(info.width, info.height, info.channels, info.format);
        MovieIO::ChannelSet channel_set = _movie_input->channel_sets().at(selected_channelset_idx.get());
        if (!_movie_input->read(image->data, channel_set))
        {
            throw "Cant read image data";
        }

        // Cache read image
        if (enable_cache.get()) _cache[CacheKey(frame.get())] = image;
    }
    else if (_cache.contains(CacheKey(frame.get())))
    {
        image = _cache[CacheKey(frame.get())];
    }

    // trigger output
    plate_out.trigger(image);
}

std::vector<std::tuple<int, int>> ReadNode::cached_range() const {
    // collect cache frame ranges
    std::vector<int> cached_frames;
    for (const auto& [cache_key, img] : _cache)
    {
        cached_frames.push_back(cache_key.frame);
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

void ReadNode::onGUI()
{

    // File ----------------------------------------------------
    ImGui::Text("File");

    std::string filename = file.get();
    if (ImGui::InputText("file", &filename))
    {
        file.set(filename);
    }
    ImGui::SameLine();

    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::GetTextLineHeightWithSpacing());
    ImGui::SetNextItemWidth(ImGui::GetTextLineHeightWithSpacing());
    if (ImGui::Button(ICON_FA_FOLDER_OPEN "##open"))
    {
        auto selected_file = glazy::open_file_dialog("all supported\0*.mp4;*.exr;*.jpg\0EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg\0MP4 movies\0*.mp4");
        if (!selected_file.empty())
        {
            file.set(selected_file.string());
        }
    }
    ImGui::LabelText("range", "%d-%d", _first_frame, _last_frame);
    ImGui::SliderInt("frame", &frame, _first_frame, _last_frame);

    ImGui::Separator(); // ---------------------------------------

    //int used_memory = 0;
    //for (const auto& [key, memory_img] : _cache) {
    //    const auto& [mem, w, h, channels, f] = memory_img;

    //    bool enable_cache_value = enable_cache.get();
    //    if (ImGui::Checkbox("enable cache", &enable_cache_value))
    //    {
    //        enable_cache.set(enable_cache_value);
    //    }

    //    int used_memory = 0;
    //    for (const auto& [key, img] : _cache)
    //    {
    //        used_memory += img->bytes();
    //    }
    //    ImGui::LabelText("images", "%d", _cache.size());
    //    ImGui::LabelText("memory", "%.2f MB", used_memory / pow(1000, 2));

    //    for (const auto& inlet : plate_out.targets())
    //    {
    //        ImGui::Text("inlet: %s", "target");
    //    }

    //    ImGui::Ranges(cached_range(), _first_frame, _last_frame);

    //    if (_current_file)
    //    {
    //        auto info = _current_file->info();
    //        ImGui::LabelText("size", "%dx%d", info.width, info.height);

    //        ImGui::LabelText("channels", "%s", join_string(info.channels, ", ").c_str());

    //        ImGui::LabelText("format", "%s", info.format.c_str());
    //    }
    //}
}