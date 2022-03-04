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
#include "../../tracy/Tracy.hpp"

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"
#include "glhelpers.h"

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


//
#include "FileSequence.h"
#include "Layer.h"
#include "helpers.h"




namespace ImGui
{
    bool Frameslider(const char* label, bool *is_playing, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0)
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
                std::cout << "oress play" << *is_playing << "\n";
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
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::GetTextLineHeight() * 2 - ImGui::GetStyle().ItemSpacing.x);
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

}


#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
inline std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;

    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;

    return r;
}
#endif

template<typename Key, typename Value>
std::vector<Key> extract_keys(std::map<Key, Value> const& input_map) {
    std::vector<Key> retval;
    for (auto const& element : input_map) {
        retval.push_back(element.first);
    }
    return retval;
}


class SequenceRenderer
{
public:
    FileSequence sequence;
    //Imf::Header header;
    void* pixels = NULL;
    std::vector<GLuint> pbos;
    std::vector<std::tuple<int, int, int, int>> pbo_data_sizes;
    bool orphaning{ true };
    GLuint tex{ 0 };

    //int width, height, nchannels;
    std::string infostring;
    Imf::MultiPartInputFile* first_file;
    std::unique_ptr<Imf::MultiPartInputFile> current_file;

    int selected_part{ 0 };
    std::vector<std::string> selected_channels{ "B", "G", "R", "A"};
    GLenum glinternalformat = GL_RGBA16F;
    GLenum glformat = GL_BGR;
    GLenum gltype = GL_HALF_FLOAT;


public:

    SequenceRenderer()
    {

    }

    void onGUI()    
    {
        if (ImGui::CollapsingHeader("Parts", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // collect exr parts (MultiPartFile)->part_names
            std::vector<std::string> part_names;
            for (auto p = 0; p < first_file->parts(); p++)
            {
                std::string part_name = first_file->header(p).hasName() ? first_file->header(p).name() : "";
                part_names.push_back(part_name);
            }

            // EXR Parts GUI
            if (ImGui::BeginCombo("parts", part_names[selected_part].c_str(), ImGuiComboFlags_NoArrowButton))
            {
                for (auto i = 0; i < part_names.size(); i++) {
                    const bool is_selected = i == selected_part;
                    if (ImGui::Selectable(part_names[i].c_str(), is_selected)) {
                        selected_part = i;
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("parts");

            // Collect all Channels in selected part
            // (file, part)->channellist
            std::vector<std::string> available_channels;
            Imf::InputPart in(*first_file, selected_part);
            auto cl = in.header().channels();
            for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
            {
                available_channels.push_back(i.name());
            }

            ImGui::Text("Available channels\n  %s", join_string(available_channels, ", ").c_str());
        }

        if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::vector<std::string> available_channels;
            Imf::InputPart in(*first_file, selected_part);
            auto cl = in.header().channels();
            for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
            {
                available_channels.push_back(i.name());
            }

            Layer lyr("");
            lyr.channels = available_channels;

            std::stringstream ss;
            ss << lyr;
            ImGui::Text("%s", ss.str().c_str());

            std::vector<Layer>sublayers;
            sublayers = lyr.group_by_delimiter();

            static bool group_by_patterns{ true };
            ImGui::Checkbox("group by patterns", &group_by_patterns);
            if(group_by_patterns) {
                const std::vector<std::vector<std::string>> patterns = { {"red", "green", "blue"},{"A", "B", "G", "R"},{"B", "G", "R"},{"x", "y"},{"u", "v"},{"u", "v", "w"} };
                std::vector<Layer> subsublyrs;
                for (const auto& sublyr : sublayers)
                {
                    for (auto subsub : sublyr.group_by_patterns(patterns)) {
                        subsublyrs.push_back(subsub);
                    }
                }
                sublayers = subsublyrs;
            }

            //static int selected_layer;
            if (ImGui::BeginListBox("layers"))
            {
                for (auto i=0; i<sublayers.size(); i++)
                {
                    std::stringstream ss;
                    ss << sublayers[i].name << "(";
                    for (auto c : sublayers[i].channels) ss << c;
                    ss << ")";

                    bool is_selected = sublayers[i].channel_ids() == selected_channels;
                    if (ImGui::Selectable(ss.str().c_str(), is_selected))
                    {

                        selected_channels = sublayers[i].channel_ids();
                    }
                }
                ImGui::EndListBox();
            }

            ImGui::LabelText("read channels", join_string(selected_channels, ", ").c_str());
        }

        if (ImGui::CollapsingHeader("data format", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Data format
            ImGui::Text("glformat: %s", to_string(glformat).c_str());
            ImGui::Text("gltype:   %s", to_string(gltype).c_str());
        }

        if (ImGui::CollapsingHeader("OpenGL", ImGuiTreeNodeFlags_DefaultOpen))
        {

            /// PBOS
            static int npbos = pbos.size();
            if (ImGui::InputInt("pbos", &npbos))
            {
                init_pbos(npbos);
            }

            ImGui::Checkbox("orphaning", &orphaning);

            /// Internalformat
            if (ImGui::BeginListBox("texture internal format"))
            {
                std::vector<GLenum> glinternalformats{ GL_RGB16F, GL_RGBA16F, GL_RGB32F, GL_RGBA32F };
                for (auto i = 0; i < glinternalformats.size(); ++i)
                {
                    const bool is_selected = glinternalformat == glinternalformats[i];
                    if (ImGui::Selectable(to_string(glinternalformats[i]).c_str(), is_selected))
                    {
                        glinternalformat = glinternalformats[i];
                        init_tex();
                    }
                }
                ImGui::EndListBox();
            }
        }
    }

    void setup(FileSequence seq)
    {
        ZoneScoped;

        sequence = seq;

        Imf::setGlobalThreadCount(32);

        ///
        /// Open file
        /// 
        auto filename = sequence.item(sequence.first_frame);
        Imf::MultiPartInputFile* file;

        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
        file = new Imf::MultiPartInputFile(*inputStdStream);
        #else
        file = new Imf::MultiPartInputFile(filename.c_str());
        #endif
        first_file = file;
        // read info to string
        infostring = get_infostring(*first_file);

        /// read header 
        int parts = file->parts();
        bool fileComplete = true;
        for (int i = 0; i < parts && fileComplete; ++i)
            if (!file->partComplete(i)) fileComplete = false;

        /// alloc 
        Imath::Box2i displayWindow = first_file->header(0).displayWindow();
        auto display_width = displayWindow.max.x - displayWindow.min.x + 1;
        auto display_height = displayWindow.max.y - displayWindow.min.y + 1;
        auto pbo_nchannels = 4;
        pixels = malloc((size_t)display_width * display_height * sizeof(half) * pbo_nchannels);
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }

        // init texture object
        init_tex();

        // init pixel buffers
        init_pbos(1);
    }

    void init_tex() {
        // init texture object
        Imath::Box2i display_window = first_file->header(0).displayWindow();
        auto display_width = display_window.max.x - display_window.min.x + 1;
        auto display_height = display_window.max.y - display_window.min.y + 1;
        glPrintErrors();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, display_width, display_height);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "init tex errors:" << "\n";
        glPrintErrors();
    }

    void init_pbos(int n)
    {
        ZoneScoped;

        Imath::Box2i displayWindow = first_file->header(0).displayWindow();
        auto display_width = displayWindow.max.x - displayWindow.min.x + 1;
        auto display_height = displayWindow.max.y - displayWindow.min.y + 1;
        auto max_nchannels = 4;

        TracyMessage(("set pbo count to: "+std::to_string(n)).c_str(), 9);
        if (!pbos.empty())
        {
            for (auto i = 0; i < pbos.size(); i++) {
                glDeleteBuffers(1, &pbos[i]);
            }
            
        }

        pbos.resize(n);
        pbo_data_sizes.reserve(n);

        for (auto i=0; i<n;i++)
        {
            GLuint pbo;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, display_width * display_height * max_nchannels * sizeof(half), 0, GL_STREAM_DRAW);
            pbos[i] = pbo;
            pbo_data_sizes[i] = {0,0, 0,0 };
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void destroy()
    {
        for (auto i = 0; i < pbos.size(); i++) {
            glDeleteBuffers(1, &pbos[i]);
        }
        glDeleteTextures(1, &tex);
    }

    void update(int F)
    {
        ZoneScoped;

        auto filename = sequence.item(F);
        if (!std::filesystem::exists(filename)) {
            std::cerr << "file does not exist: " << filename << "\n";
            return;
        }

        // open current file

        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        std::unique_ptr<std::ifstream> inputStr;
        std::unique_ptr<Imf::StdIFStream> inputStdStream;
        #endif
        try
        {
            ZoneScopedN("OpenFile");
            #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
            inputStr = std::make_unique<std::ifstream>(s2ws(filename.string()), std::ios_base::binary);
            inputStdStream = std::make_unique<Imf::StdIFStream>(*inputStr, filename.string().c_str());
            current_file = std::make_unique<Imf::MultiPartInputFile>(*inputStdStream);
            #else
            current_file = std::make_unique<Imf::MultiPartInputFile>(filename.c_str());
            #endif
        }
        catch (const Iex::InputExc& ex) {
            std::cerr << "file doesn't appear to really be an EXR file" << "\n";
            std::cerr << "  " << ex.what() << "\n";
            return;
        }

        //ImGui::Text("current filename: %s", filename.string().c_str());

        // Collect all available channels
        //std::vector<std::string> available_channels;
        Imf::InputPart in(*current_file, selected_part);
        //auto cl = in.header().channels();
        //for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
        //{
        //    available_channels.push_back(i.name());
        //}

        //ImGui::Text("Available channels\n  %s", join_string(available_channels, ", ").c_str());

        /// read pixels to pixels PBO
        //Imf::InputPart in(*current_file, selected_part);
        Imath::Box2i dataWindow = in.header().dataWindow();
        auto data_width = dataWindow.max.x - dataWindow.min.x + 1;
        auto data_height = dataWindow.max.y - dataWindow.min.y + 1;
        auto nchannels = selected_channels.size();
        int dx = dataWindow.min.x;
        int dy = dataWindow.min.y;

        //ImGui::Text("data window: (%d %d) %d x %d", dx, dy, data_width, data_height);
        const auto display_channels_from_selected_channels = [](const std::vector<std::string>& channels) {
            // Reorder Alpha
            // EXR sort layer names in alphabetically order, therefore the alpha channel comes before RGB
            // if exr contains alpha move this channel to the 4th position or the last position.

            // find alpha channel index
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

            // move alpha channel to 4th idx
            std::vector<std::string> reordered_channels{ channels };
            ImGui::LabelText("HasAlpha", AlphaIndex >= 0 ? "yes" : "no");
            if (AlphaIndex >= 0) {
                reordered_channels.erase(reordered_channels.begin() + AlphaIndex);
                reordered_channels.insert(std::min(reordered_channels.begin() + 3, reordered_channels.end()), alpha_channel_name);
            }
            return reordered_channels;
        };

        auto display_channels = display_channels_from_selected_channels(selected_channels);
        {
            ZoneScopedN("ReadPixels");
            Imf::FrameBuffer frameBuffer;
            //size_t chanoffset = 0;
            unsigned long long xstride = sizeof(half) * nchannels;
            char* buf = (char*)pixels - (dx * xstride + dy * data_width * xstride);

            for (auto i = 0; i < display_channels.size(); i++) {
                auto name = display_channels[i];
                //size_t chanbytes = sizeof(half);
                frameBuffer.insert(name,   // name
                    Imf::Slice(Imf::PixelType::HALF, // type
                        buf + i * sizeof(half),           // base
                        xstride,
                        (size_t)data_width * xstride
                    )
                );
                //chanoffset += chanbytes;
            }

            in.setFrameBuffer(frameBuffer);
            in.readPixels(dataWindow.min.y, dataWindow.max.y);
        }

        ///
        /// Transfer pixels to Texture
        /// 
        // Match glformat and swizzle to data
        // this is a very important step. It depends on the framebuffer channel order.
        // exr order channel names in aplhebetic order. So by default, ABGR is the read order.
        // upstream this can be changed, therefore we must handle various channel orders eg.: RGBA, BGRA, and alphebetically sorted eg.: ABGR channel orders.

        std::array<GLint, 4> swizzle_mask{ GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };

        if (display_channels == std::vector<std::string>({ "B", "G", "R", "A" }))
        {
            glformat = GL_BGRA;
            swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
        }
        else if (display_channels == std::vector<std::string>({ "R", "G", "B", "A" }))
        {
            glformat = GL_BGRA;
            swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
        }
        else if (display_channels == std::vector<std::string>({ "A", "B", "G", "R" }))
        {
            glformat = GL_BGRA;
            swizzle_mask = { GL_ALPHA, GL_RED, GL_GREEN, GL_BLUE };
        }
        else if (display_channels == std::vector<std::string>({ "B", "G", "R" }))
        {
            glformat = GL_BGR;
            swizzle_mask = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
        }
        else if (display_channels.size() == 4)
        {
            glformat = GL_BGRA;
            swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE , GL_ALPHA };
        }
        else if (display_channels.size() == 3)
        {
            glformat = GL_RGB;
            swizzle_mask = { GL_RED , GL_GREEN, GL_BLUE, GL_ONE };
        }
        else if (display_channels.size() == 2) {
            glformat = GL_RG;
            swizzle_mask = { GL_RED, GL_GREEN, GL_ZERO, GL_ONE };
        }
        else if (display_channels.size() == 1)
        {
            glformat = GL_RED;
            if (display_channels[0] == "R") swizzle_mask = { GL_RED, GL_ZERO, GL_ZERO, GL_ONE };
            else if (display_channels[0] == "G") swizzle_mask = { GL_ZERO, GL_RED, GL_ZERO, GL_ONE };
            else if (display_channels[0] == "B") swizzle_mask = { GL_ZERO, GL_ZERO, GL_RED, GL_ONE };
            else swizzle_mask = { GL_RED, GL_RED, GL_RED, GL_ONE };  
        }
        else {
            throw "cannot match selected channels to glformat";
        }

        //ImGui::Text("swizzle mask: %s %s %s %s", to_string(swizzle_mask[0]), to_string(swizzle_mask[1]), to_string(swizzle_mask[2]), to_string(swizzle_mask[3]));

        if (pbos.empty())
        {
            ZoneScopedN("pixels to texture");
            // pixels to texture
            glInvalidateTexImage(tex, 1);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, data_width, data_height, glformat, GL_HALF_FLOAT, pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else
        {
            ZoneScopedN("pixels to texture");
            static int index{ 0 };
            static int nextIndex;

            index = (index + 1) % pbos.size();
            nextIndex = (index + 1) % pbos.size();

            //ImGui::Text("using PBOs, update:%d  display: %d", index, nextIndex);

            // pbo to texture
            glBindTexture(GL_TEXTURE_2D, tex);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[index]);
            if(orphaning) glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, std::get<2>(pbo_data_sizes[index]), std::get<3>(pbo_data_sizes[index]), glformat, GL_HALF_FLOAT, 0/*NULL offset*/); // orphaning

            // channel swizzle
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());

            //// pixels to other PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[nextIndex]);
            pbo_data_sizes[nextIndex] = {dx, dy, data_width, data_height };
            glBufferData(GL_PIXEL_UNPACK_BUFFER, data_width * data_height * nchannels * sizeof(half), 0, GL_STREAM_DRAW);
            // map the buffer object into client's memory
            GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr)
            {
                ZoneScopedN("pixels to pbo");
                // update data directly on the mapped buffer
                memcpy(ptr, pixels, data_width * data_height * nchannels * sizeof(half));
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
            }
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void draw()
    {
        ZoneScoped;
        glClearColor(1, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        //imdraw::quad(glazy::checkerboard_tex);
        //std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
        imdraw::quad(tex);
    }

    ~SequenceRenderer() {
        destroy();
    }
};

int main()
{

    //return Test::test_layer_class();

    bool is_playing = false;
    int F, first_frame, last_frame;
    int selected_part{ 0 };
    FileSequence sequence{ "C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr" };
    //FileSequence sequence{ "C:/Users/andris/Desktop/52_EXAM_v51-raw/52_01_EXAM_v51.0001.exr" };
    //FileSequence sequence{ "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr" };
    first_frame = sequence.first_frame;
    last_frame = sequence.last_frame;
    F = sequence.first_frame;

    glazy::init();

    SequenceRenderer seq_renderer;
    seq_renderer.setup(sequence);

    glazy::set_vsync(false);
    while (glazy::is_running())
    {
        if (is_playing)
        {
            F++;
            if (F > last_frame) F = first_frame;
        }

        glazy::new_frame();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("file"))
            {
                if (ImGui::MenuItem("open", "")) {
                    auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg");
                    sequence = FileSequence(filepath);
                    first_frame = sequence.first_frame;
                    last_frame = sequence.last_frame;
                    F = sequence.first_frame;
                    seq_renderer.destroy();
                    seq_renderer.setup(sequence);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (ImGui::Begin("Info"))
        {
            if (ImGui::BeginTabBar("info")) {
                if (ImGui::BeginTabItem("channels"))
                {
                    auto parts = seq_renderer.first_file->parts();
                    for (auto p = 0; p < parts; p++)
                    {
                        ImGui::PushID(p);
                        auto in = Imf::InputPart(*seq_renderer.first_file, p);
                        auto header = in.header();
                        auto cl = header.channels();
                        ImGui::Text("  part %d\n name: %s\n view: %s", p, header.hasName() ? header.name().c_str() : "", header.hasView() ? header.view().c_str() : "");


                        if (ImGui::BeginTable("channels table", 4))
                        {
                            ImGui::TableSetupColumn("name");
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("sampling");
                            ImGui::TableSetupColumn("linear");
                            ImGui::TableHeadersRow();
                            for (Imf::ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
                            {
                                ImGui::PushID(p);
                                //ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                bool is_selected = (seq_renderer.selected_part == p) && std::find(seq_renderer.selected_channels.begin(), seq_renderer.selected_channels.end(), std::string(i.name())) != seq_renderer.selected_channels.end();
                                if (ImGui::Selectable(i.name(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                                {
                                    seq_renderer.selected_part = p;
                                    auto channel_name = std::string(i.name());
                                    seq_renderer.selected_channels = {channel_name};
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

                if (ImGui::BeginTabItem("info (current file)"))
                {
                    ImGui::TextWrapped(get_infostring(*seq_renderer.current_file).c_str());
                    ImGui::EndTabItem();
                }
            }

        }
        ImGui::End();


        if (ImGui::Begin("Inspector"))
        {
            seq_renderer.onGUI();
        }
        ImGui::End();

        if (ImGui::Begin("Timeline"))
        {
            ImGui::Frameslider("timeslider", &is_playing, &F, first_frame, last_frame);
        }
        ImGui::End();

        seq_renderer.update(F);
        seq_renderer.draw();

        glazy::end_frame();
        FrameMark;
    }
    glazy::destroy();
}
