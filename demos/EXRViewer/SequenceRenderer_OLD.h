#pragma once

class SequenceRenderer
{
private:
    // PIXELS READED
    // read file to memory address
    // inputs:
    FileSequence sequence;

    std::vector<std::string> mSelectedChannels{ "A", "B", "G", "R" }; // channels to actually read
    void* pixels = NULL; // tha address to the pixels to read file into
    //int data_x, data_y, data_width, data_height; // the actial pixels data and display dimensions 
    std::tuple<int, int, int, int> bbox;


    // private cache
public:
    int current_frame{ 0 }; // selected frame to read
    int selected_part_idx{ 0 }; // selected part to read
    //int display_x, display_y, display_width, display_height;

    std::unique_ptr<SequenceReader> reader;

private:

    // PIXELS RENDERER
    // Performance options
    bool orphaning{ true };

    // Private cache
    GLenum glformat = GL_BGR; // from channels
    GLenum gltype = GL_HALF_FLOAT; // from datatype
    GLuint data_tex{ 0 }; // datatexture
public:
    GLenum glinternalformat = GL_RGBA16F; // selec texture internal format
// output

    GLuint fbo{ 0 };
    GLuint color_attachment{ 0 };
    std::filesystem::path mVertexPath;
    std::filesystem::path mFragmentPath;
    GLuint mProgram;

    std::vector<GLuint> pbos;
    std::vector<std::tuple<int, int, int, int>> pbo_data_sizes; // keep PBOs dimension

    /// set channels for display. 
    /// proces channels arg: keep maximum 4 channels. reorder alpha etc.
    void set_selected_channels(std::vector<std::string> channels)
    {
        mSelectedChannels = get_display_channels(channels);
    }

    auto selected_channels() {
        return mSelectedChannels;
    }
private:
    static int alpha_index(std::vector<std::string> channels) {
        /// find alpha channel index
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

    static int alpha_channel(const std::vector<std::string>& channels) {
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

    static std::vector<std::string> get_display_channels(const std::vector<std::string>& channels)
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

    void init_pixels(int width, int height, int nchannels, size_t typesize)
    {
        pixels = malloc((size_t)width * height * nchannels * sizeof(half));
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }
    }

    void init_pbos(int width, int height, int channels, int n)
    {
        ZoneScoped;

        TracyMessage(("set pbo count to: " + std::to_string(n)).c_str(), 9);
        if (!pbos.empty())
        {
            for (auto i = 0; i < pbos.size(); i++) {
                glDeleteBuffers(1, &pbos[i]);
            }

        }

        pbos.resize(n);
        pbo_data_sizes.resize(n);

        for (auto i = 0; i < n; i++)
        {
            GLuint pbo;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * channels * sizeof(half), 0, GL_STREAM_DRAW);
            pbos[i] = pbo;
            pbo_data_sizes[i] = { 0,0, 0,0 };
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void init_tex(int width, int height) {
        // init texture object

        glPrintErrors();
        glGenTextures(1, &data_tex);
        glBindTexture(GL_TEXTURE_2D, data_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexStorage2D(GL_TEXTURE_2D, 1, glinternalformat, width, height);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "init tex errors:" << "\n";
        glPrintErrors();
    }

    void init_fbo(int width, int height)
    {
        color_attachment = imdraw::make_texture_float(width, height, 0, glinternalformat, glformat, gltype);
        fbo = imdraw::make_fbo(color_attachment);
    }

    void init_program() {
        ZoneScoped;
        std::cout << "recompile checker shader" << "\n";

        // reload shader code
        auto vertex_code = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            void main()
            {
                gl_Position = vec4(aPos, 1.0);
            }
        )";

        auto fragment_code = R"(
            #version 330 core
            out vec4 FragColor;
            uniform mediump sampler2D inputTexture;
            uniform vec2 resolution;
            uniform ivec4 bbox;

            void main()
            {
                vec2 uv = (gl_FragCoord.xy)/resolution; // normalize fragcoord
                uv=vec2(uv.x, 1.0-uv.y); // flip y
                uv-=vec2(bbox.x/resolution.x, bbox.y/resolution); // position image
                vec3 color = texture(inputTexture, uv).rgb;
                float alpha = texture(inputTexture, uv).a;
                FragColor = vec4(color, alpha);
            }
        )";

        // link new program
        GLuint program = imdraw::make_program_from_source(
            vertex_code,
            fragment_code
        );

        // swap programs
        if (glIsProgram(mProgram)) glDeleteProgram(mProgram);
        mProgram = program;
    }

public:

    SequenceRenderer(const FileSequence& seq)
    {
        ZoneScoped;

        sequence = seq;
        current_frame = sequence.first_frame;
        Imf::setGlobalThreadCount(24);

        reader = std::make_unique<SequenceReader>(seq);
        reader->size();
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
        //Imath::Box2i display_window = first_file->header(selected_part_idx).displayWindow();
        //display_x = display_window.min.x;
        //display_y = display_window.min.y;
        //display_width = display_window.max.x - display_window.min.x + 1;
        //display_height = display_window.max.y - display_window.min.y + 1;

        auto [display_width, display_height] = reader->size();
        /// alloc space for pixels
        init_pixels(display_width, display_height, 4, sizeof(half));

        // init pixel buffers
        init_pbos(display_width, display_height, 4, 3);

        // init texture object
        init_tex(display_width, display_height);

        //
        init_fbo(display_width, display_height);

        //
        init_program();
    }

    void set_uniforms(std::map<std::string, imdraw::UniformVariant> uniforms) {
        imdraw::set_uniforms(mProgram, uniforms);
    }

    void onGUI()
    {
        if (ImGui::CollapsingHeader("performance", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int thread_count = Imf::globalThreadCount();
            if (ImGui::InputInt("global thread count", &thread_count)) {
                Imf::setGlobalThreadCount(thread_count);
            }
        }
        if (ImGui::CollapsingHeader("attributes", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragInt("current frame", &current_frame);
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
                auto [display_width, display_height] = reader->size();
                init_pbos(display_width, display_height, 4, npbos);
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
                        auto [display_width, display_height] = reader->size();
                        init_tex(display_width, display_height);
                    }
                }
                ImGui::EndListBox();
            }
        }
    }

    void update()
    {
        ZoneScoped;
        if (mSelectedChannels.empty()) return;

        /// Open Current InputPart
        std::unique_ptr<Imf::MultiPartInputFile> current_file;
        std::unique_ptr<Imf::InputPart> current_inputpart;
        {
            ZoneScopedN("Open Curren InputPart");
            auto filename = sequence.item(current_frame);
            //if (!std::filesystem::exists(filename)) {
            //    std::cerr << "file does not exist: " << filename << "\n";
            //    return;
            //}

            try
            {
                ZoneScopedN("Open Current File");
                current_file = std::make_unique<Imf::MultiPartInputFile>(filename.string().c_str());

            }
            catch (const Iex::InputExc& ex) {
                std::cerr << "file doesn't appear to really be an EXR file" << "\n";
                std::cerr << "  " << ex.what() << "\n";
                return;
            }

            {
                ZoneScopedN("seek part");
                current_inputpart = std::make_unique<Imf::InputPart>(*current_file, selected_part_idx);
            }
        }

        /// Update datawindow
        {
            ZoneScopedN("Update datawindow");
            Imath::Box2i dataWindow = current_inputpart->header().dataWindow();
            bbox = std::tuple<int, int, int, int>(
                dataWindow.min.x,
                dataWindow.min.y,
                dataWindow.max.x - dataWindow.min.x + 1,
                dataWindow.max.y - dataWindow.min.y + 1);
        }

        ///
        /// Transfer pixels to Texture
        /// 

        std::array<GLint, 4> swizzle_mask{ GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
        glformat = glformat_from_channels(mSelectedChannels, swizzle_mask);

        if (pbos.empty())
        {
            // Read pixels to memory
            //exr_to_memory(sequence.item(current_frame), selected_part_idx, selected_channels, &bbox, pixels);
            auto [x, y, w, h] = bbox;
            exr_to_memory(*current_inputpart, x, y, w, h, mSelectedChannels, pixels);

            ZoneScopedN("pixels to texture");
            // pixels to texture
            memory_to_texture(pixels, 0, 0, w, h, mSelectedChannels, GL_HALF_FLOAT, data_tex);

            glInvalidateTexImage(data_tex, 1);
            glBindTexture(GL_TEXTURE_2D, data_tex);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glformat, GL_HALF_FLOAT, pixels);
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

            /// pbo to texture
            {
                glBindTexture(GL_TEXTURE_2D, data_tex);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[index]);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, std::get<2>(pbo_data_sizes[index]), std::get<3>(pbo_data_sizes[index]), glformat, GL_HALF_FLOAT, 0/*NULL offset*/); // orphaning
                glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            /// texture with image to FBO
            {
                auto [display_width, display_height] = reader->size();
                BeginRenderToTexture(fbo, 0, 0, display_width, display_height);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                auto [x, y, w, h] = pbo_data_sizes[index];
                set_uniforms({
                    {"inputTexture", 0},
                    {"resolution", glm::vec2(display_width, display_height)},
                    {"bbox", glm::ivec4(x,y,w,h)}
                    });

                /// Create geometry
                static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
                static auto vao = imdraw::make_vao(mProgram, { {"aPos", {vbo, 3}} });

                /// Draw quad with fragment shader
                imdraw::push_program(mProgram);
                glBindTexture(GL_TEXTURE_2D, data_tex);
                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
                imdraw::pop_program();
                EndRenderToTexture();
            }

            /// pixels to other PBO
            {
                auto [display_width, display_height] = reader->size();
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[nextIndex]);
                pbo_data_sizes[nextIndex] = bbox;
                glBufferData(GL_PIXEL_UNPACK_BUFFER, display_width * display_height * mSelectedChannels.size() * sizeof(half), 0, GL_STREAM_DRAW);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }

            /// exr to pbo
            static bool ReadDirectlyToPBO{ true };
            //ImGui::Checkbox("ReadDirectlyToPBO", &ReadDirectlyToPBO);
            if (ReadDirectlyToPBO)
            {
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[nextIndex]);
                //pbo_data_sizes[nextIndex] = { data_x, data_y, data_width, data_height };
                //glBufferData(GL_PIXEL_UNPACK_BUFFER, data_width * data_height * mSelectedChannels.size() * sizeof(half), 0, GL_STREAM_DRAW);
                GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                if (!ptr) std::cerr << "cannot map PBO" << "\n";
                if (ptr != NULL)
                {
                    auto [x, y, w, h] = pbo_data_sizes[nextIndex];
                    exr_to_memory(*current_inputpart, x, y, w, h, mSelectedChannels, ptr);
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                }
            }
            else {
                auto [x, y, w, h] = pbo_data_sizes[nextIndex];
                exr_to_memory(*current_inputpart, x, y, w, h, mSelectedChannels, pixels);

                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[nextIndex]);
                //pbo_data_sizes[nextIndex] = { data_x, data_y, data_width, data_height };
                //glBufferData(GL_PIXEL_UNPACK_BUFFER, data_width* data_height* mSelectedChannels.size() * sizeof(half), 0, GL_STREAM_DRAW);
                GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                if (!ptr) std::cerr << "cannot map PBO" << "\n";
                if (ptr != NULL)
                {
                    auto [x, y, w, h] = pbo_data_sizes[nextIndex];
                    memcpy(ptr, pixels, w * h * mSelectedChannels.size() * sizeof(half));
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                }
            }
        }
    }

    void render()
    {
        if (mSelectedChannels.empty()) return;
        ZoneScoped;
        auto [display_width, display_height] = reader->size();
        imdraw::quad(data_tex, { 0,0 }, { display_width, display_height });
    }

    ~SequenceRenderer() {
        for (auto i = 0; i < pbos.size(); i++) {
            glDeleteBuffers(1, &pbos[i]);
        }
        glDeleteTextures(1, &data_tex);
    }
};