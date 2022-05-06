#pragma once

#include "EXRSequenceReader.h"
#include "stringutils.h"
//#include "../../tracy/Tracy.hpp"

// OpenEXR
#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfStdIO.h>
#include <OpenEXR/ImfArray.h> //Imf::Array2D
#include <OpenEXR/half.h> // <half> type
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfVersion.h> // get version
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>

#include "imgui.h"

int alpha_channel(const std::vector<std::string>& channels) {
    int AlphaIndex{ -1 };
    std::string alpha_channel_name{};
    for (auto i = 0; i < channels.size(); i++)
    {
        auto channel_name = channels[i];
        if (ends_with(channel_name, "A") || ends_with(channel_name, "a") || ends_with(channel_name, "alpha")) {
            AlphaIndex = i;
            alpha_channel_name = channel_name;
            break;
        }
    }
    return AlphaIndex;
}

std::vector<std::string> get_display_channels(const std::vector<std::string>& channels)
{
    // Reorder Alpha
    // EXR sort layer names in alphabetically order, therefore the alpha channel comes before RGB
    // if exr contains alpha move this channel to the 4th position or the last position.
    auto AlphaIndex = alpha_channel(channels);

    // move alpha channel to 4th idx
    std::vector<std::string> reordered_channels{ channels };
    if (AlphaIndex >= 0) {
        reordered_channels.erase(reordered_channels.begin() + AlphaIndex);
        reordered_channels.insert(reordered_channels.begin() + std::min((size_t)3, reordered_channels.size()), channels[AlphaIndex]);
    }

    // keep first 4 channels only
    if (reordered_channels.size() > 4) {
        reordered_channels = std::vector<std::string>(reordered_channels.begin(), reordered_channels.begin() + 4);
    }
    return reordered_channels;
};

EXRSequenceReader::EXRSequenceReader(const FileSequence& seq)
{
    //ZoneScoped;

    sequence = seq;
    current_frame = sequence.first_frame;
    Imf::setGlobalThreadCount(24);

    ///
    /// Open file
    /// 
    auto filename = sequence.item(sequence.first_frame);

    auto first_file = std::make_unique<Imf::MultiPartInputFile>(filename.string().c_str());

    // read info to string
    //infostring = get_infostring(*first_file);
    int parts = first_file->parts();
    bool fileComplete = true;
    for (int i = 0; i < parts && fileComplete; ++i)
        if (!first_file->partComplete(i)) fileComplete = false;

    /// read header 
    Imath::Box2i display_window = first_file->header(selected_part_idx).displayWindow();
    display_x = display_window.min.x;
    display_y = display_window.min.y;
    display_width = display_window.max.x - display_window.min.x + 1;
    display_height = display_window.max.y - display_window.min.y + 1;
}

void EXRSequenceReader::onGUI() {

    int thread_count = Imf::globalThreadCount();
    if (ImGui::InputInt("global thread count", &thread_count)) {
        Imf::setGlobalThreadCount(thread_count);
    }

    ImGui::DragInt("current frame", &current_frame);

}

/// display size
std::tuple<int, int> EXRSequenceReader::size()
{
    return { display_width, display_height };
}

std::tuple<int, int, int, int> EXRSequenceReader::bbox()
{
    return m_bbox;
}

void EXRSequenceReader::set_selected_channels(std::vector<std::string> channels)
{
    mSelectedChannels = get_display_channels(channels);
}

std::vector<std::string> EXRSequenceReader::selected_channels() {
    return mSelectedChannels;
}

void EXRSequenceReader::read_to_memory(void* memory)
{
    //ZoneScoped;
    if (mSelectedChannels.empty()) return;

    /// Open Current InputPart
    std::unique_ptr<Imf::MultiPartInputFile> current_file;
    std::unique_ptr<Imf::InputPart> current_inputpart;
    {
        //ZoneScopedN("Open Curren InputPart");

        auto filename = sequence.item(current_frame);
        if (!std::filesystem::exists(filename)) {
            std::cerr << "file does not exist: " << filename << "\n";
            return;
        }

        try
        {
            //ZoneScopedN("Open Current File");
            current_file = std::make_unique<Imf::MultiPartInputFile>(filename.string().c_str());
        }
        catch (const Iex::InputExc& ex)
        {
            std::cerr << "file doesn't appear to really be an EXR file" << "\n";
            std::cerr << "  " << ex.what() << "\n";
            return;
        }

        {
            //ZoneScopedN("seek part");
            current_inputpart = std::make_unique<Imf::InputPart>(*current_file, selected_part_idx);
        }
    }

    /// Update datawindow
    {
        //ZoneScopedN("Update datawindow");
        Imath::Box2i dataWindow = current_inputpart->header().dataWindow();
        m_bbox = std::tuple<int, int, int, int>(
            dataWindow.min.x,
            dataWindow.min.y,
            dataWindow.max.x - dataWindow.min.x + 1,
            dataWindow.max.y - dataWindow.min.y + 1);
    }

    // read pixels to pointer
    auto [x, y, w, h] = m_bbox;

    Imf::FrameBuffer frameBuffer;
    //size_t chanoffset = 0;
    unsigned long long xstride = sizeof(half) * mSelectedChannels.size();
    char* buf = (char*)memory;
    buf -= (x * xstride + y * w * xstride);

    size_t chanoffset = 0;
    for (auto name : mSelectedChannels)
    {
        size_t chanbytes = sizeof(half);
        frameBuffer.insert(name,   // name
            Imf::Slice(Imf::PixelType::HALF, // type
                buf + chanoffset,           // base
                xstride,
                (size_t)w * xstride
            )
        );
        chanoffset += chanbytes;
    }

    current_inputpart->setFrameBuffer(frameBuffer);
    current_inputpart->readPixels(y, y + h - 1);
}

