#pragma once

//#include <../tracy/Tracy.hpp>

#pragma region RenderToTexture
std::vector<GLuint> fbo_stack;
std::vector<std::array<GLint, 4>> viewport_stack;

void BeginRenderToTexture(GLuint fbo, GLint x, GLint y, GLsizei width, GLsizei height)
{
    // push fbo
    GLint current_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo); // TODO: getting fbo is fairly harmful to performance.
    fbo_stack.push_back(current_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // push viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    viewport_stack.push_back({ viewport[0], viewport[1], viewport[2], viewport[3] });
    glViewport(x, y, width, height);
}

void EndRenderToTexture()
{
    ASSERT(("Mismatch Begin/End Rendertargets", !fbo_stack.empty()));

    // restore viewport
    auto last_viewport = viewport_stack.back();
    viewport_stack.pop_back();
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);

    // restore fbo
    GLuint last_fbo = fbo_stack.back();
    glBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
    fbo_stack.pop_back();
}
#pragma endregion RenderToTexture

bool ray_ground_intersection(const glm::vec3& ray_origin, const glm::vec3& ray_dir, float *t)
{
    *t = -ray_origin.z / ray_dir.z;
    return *t > 0.0;
}

// group Channels df to nested tree by layers and views
std::map<std::string, std::map<std::string, ChannelsTable>> get_channels_table_tree(const ChannelsTable& df) {
    std::map<std::string, std::map<std::string, ChannelsTable>> channels_tree;
    for (const auto& [layer, channels] : group_by_layer(df)) {
        channels_tree[layer] = group_by_view(channels);
    }
    return channels_tree;
}

namespace ImGui {
    struct InputTextCallback_UserData
    {
        std::string* Str;
        ImGuiInputTextCallback  ChainCallback;
        void* ChainCallbackUserData;
    };

    static int InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            // Resize string callback
            // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
            std::string* str = user_data->Str;
            IM_ASSERT(data->Buf == str->c_str());
            str->resize(data->BufTextLen);
            data->Buf = (char*)str->c_str();
        }
        else if (user_data->ChainCallback)
        {
            // Forward to user callback, if any
            data->UserData = user_data->ChainCallbackUserData;
            return user_data->ChainCallback(data);
        }
        return 0;
    }

    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
    }

    bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return InputTextMultiline(label, (char*)str->c_str(), str->capacity() + 1, size, flags, InputTextCallback, &cb_user_data);
    }

    bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return InputTextWithHint(label, hint, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
    }
}

/// <summary>
/// 
/// </summary>
/// <returns>internalformat, format, type</returns>
std::tuple<GLint, GLenum, GLenum> typespec_to_opengl(const OIIO::ImageSpec& spec, int nchannels)
{
    GLint glinternalformat; 
    GLenum glformat;
    GLenum gltype;

    switch (spec.format.basetype) {
    case OIIO::TypeDesc::FLOAT: gltype = GL_FLOAT; break;
    case OIIO::TypeDesc::HALF: gltype = GL_HALF_FLOAT; break;
    case OIIO::TypeDesc::INT: gltype = GL_INT; break;
    case OIIO::TypeDesc::UINT: gltype = GL_UNSIGNED_INT; break;
    case OIIO::TypeDesc::INT16: gltype = GL_SHORT; break;
    case OIIO::TypeDesc::UINT16: gltype = GL_UNSIGNED_SHORT; break;
    case OIIO::TypeDesc::INT8: gltype = GL_BYTE; break;
    case OIIO::TypeDesc::UINT8: gltype = GL_UNSIGNED_BYTE; break;
    default:
        gltype = GL_UNSIGNED_BYTE;  // punt
        break;
    }

    bool issrgb = spec.get_string_attribute("oiio:ColorSpace") == "sRGB";

    glinternalformat = nchannels;
    if (nchannels == 1) {
        glformat = GL_LUMINANCE;
        if (spec.format.basetype == OIIO::TypeDesc::UINT8) {
            glinternalformat = GL_R8;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::UINT16) {
            glinternalformat = GL_R16F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::FLOAT)
        {
            glinternalformat = GL_R32F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::HALF) {
            glinternalformat = GL_R16F;
        }
    }
    else if (nchannels == 2) {
        glformat = GL_LUMINANCE_ALPHA;
        if (spec.format.basetype == OIIO::TypeDesc::UINT8) {
            
            glinternalformat = GL_RG8;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::UINT16) {
            glinternalformat = GL_RG16F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::FLOAT)
        {
            glinternalformat = GL_RG32F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::HALF) {
            glinternalformat = GL_RG16F;
        }
    }
    else if (nchannels == 3) {
        glformat = GL_RGB;
        if (spec.format.basetype == OIIO::TypeDesc::UINT8) {
            glinternalformat = GL_RGB8;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::UINT16) {
            glinternalformat = GL_RGB16;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::FLOAT) {
            glinternalformat = GL_RGB32F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::HALF) {
            glinternalformat = GL_RGB16F;
        }
    }
    else if (nchannels == 4) {
        glformat = GL_RGBA;
        if (spec.format.basetype == OIIO::TypeDesc::UINT8) {
            glinternalformat = GL_RGBA8;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::UINT16) {
            glinternalformat = GL_RGBA16;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::FLOAT) {
            glinternalformat = GL_RGBA32F;
        }
        else if (spec.format.basetype == OIIO::TypeDesc::HALF) {
            glinternalformat = GL_RGBA16F;
        }
    }
    else {
        glformat = GL_INVALID_ENUM;
        glinternalformat = GL_INVALID_ENUM;
    }

    return { glinternalformat, glformat, gltype };
}

GLuint make_texture_from_file(OIIO::ImageCache* image_cache, const std::filesystem::path& filename, const std::vector<ChannelKey>& channel_keys = {})
{
    ZoneScoped;
    /// Validate parameters
    if (!std::filesystem::exists(filename)) return 0;
    if (channel_keys.empty()) return 0;
    assert(("cant create a texture from more than 4 channels", channel_keys.size() <= 4));

    /// Read header
    OIIO::ImageSpec spec;
    {
        ZoneScopedN("get spec");
        image_cache->get_imagespec(OIIO::ustring(filename.string()), spec, std::get<0>(channel_keys[0]), 0);
    }
    auto x = spec.x;
    auto y = spec.y;
    int w = spec.width;
    int h = spec.height;
    int chbegin = std::get<1>(channel_keys[0]);
    int chend = std::get<1>(channel_keys[channel_keys.size() - 1]) + 1; // channel range is exclusive [0-3)
    int nchannels = chend - chbegin;

    /// Allocate and read pixels
    void* data;
    {
        ZoneScopedN("Allocate");
        auto type_size = spec.format.size();
        data = malloc(w * h * nchannels * type_size);
    }

    {
        ZoneScopedN("Read pixels");
        image_cache->get_pixels(OIIO::ustring(filename.string()), std::get<0>(channel_keys[0]), 0, x, x + w, y, y + h, 0, 1, chbegin, chend, spec.format, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, chbegin, chend);
    }

    /// Create texture for ROI
    GLuint tex_roi;
    {
        ZoneScopedN("Upload image to GPU")
        glGenTextures(1, &tex_roi);
        glBindTexture(GL_TEXTURE_2D, tex_roi);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        auto [internalformat, format, type] = typespec_to_opengl(spec, nchannels);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, spec.width, spec.height, 0, format, type, data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Deallocate data after uploaded to GPU
    {
        ZoneScopedN("Deallocate data");
        free(data);
    }

    /// Render ROI texture to full display window
    GLuint tex_full;
    {
        ZoneScopedN("Create full texture");
        tex_full = imdraw::make_texture_float(spec.full_width, spec.full_height, NULL, GL_RGBA32F);
        GLuint fbo_full = imdraw::make_fbo(tex_full);
        BeginRenderToTexture(fbo_full, spec.full_x, spec.full_y, spec.full_width, spec.full_height);
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            static GLuint prog = imdraw::make_program_from_source(R"(
                #version 330 core
                layout (location = 0) in vec3 aPos;
                void main()
                {
                    gl_Position = vec4(aPos, 1.0);
                }
            )",
                R"(
                #version 330 core
                uniform sampler2D textureMap;

                uniform ivec2 uPos;
                uniform ivec2 uSize;
                uniform ivec2 uFullSize;
                uniform bool use_sRGB_to_linear_conversion;

                float sRGB_to_linear(float sRGB_value){
                    return sRGB_value <= 0.04045
                        ? sRGB_value / 12.92
                        : pow((sRGB_value + 0.055) / 1.055, 2.4);
                }

                vec3 sRGB_to_linear(vec3 sRGB){
                    return vec3(
                        sRGB_to_linear(sRGB.r),
                        sRGB_to_linear(sRGB.g),
                        sRGB_to_linear(sRGB.b)
                        );
                }

                void main()
                {
                    vec2 uv = (gl_FragCoord.xy-uPos)/uSize;
                    vec3 raw_color = texture(textureMap, uv).rgb;
                    float alpha = texture(textureMap, uv).a;
                    vec3 color;
                    if(use_sRGB_to_linear_conversion)
                    {
                        color = sRGB_to_linear(raw_color);
                    }else{
                        color = raw_color;
                    }
                
                    gl_FragColor = vec4(color,alpha);
                }
            )");

            static auto geo = imgeo::quad();
            static auto vao = imdraw::make_vao(prog, {
                {"aPos", {imdraw::make_vbo(geo.positions), 3}},
                });

            bool use_sRGB_to_linear_conversion = spec.get_string_attribute("oiio:ColorSpace") != "Linear";
            //std::cout << "make_texture_from_file" << "\n";
            //std::cout << "  colorspace: " << spec.get_string_attribute("oiio:ColorSpace").c_str() << "\n";
            //std::cout << "  use_sRGB_to_linear_conversion: " << use_sRGB_to_linear_conversion << "\n";

            imdraw::push_program(prog);
            imdraw::set_uniforms(prog,
                {
                {"uPos", glm::ivec2(spec.x, spec.y)},
                {"uSize", glm::ivec2(spec.width, spec.height)},
                {"uFullSize", glm::ivec2(spec.full_width,spec.full_height) },
                {"use_sRGB_to_linear_conversion", use_sRGB_to_linear_conversion}
                });

            // draw
            ZoneNamedN(DRAW_ROI_ON_FULL, "Draw ROI texture to full", true);
            glBindTexture(GL_TEXTURE_2D, tex_roi);
            imdraw::draw(geo.mode, vao, imdraw::make_ebo(geo.indices), geo.indices.size());
            glBindTexture(GL_TEXTURE_2D, 0);
            imdraw::pop_program();
        }
        EndRenderToTexture();

        // Delete temporary FBO and tetures
        glDeleteFramebuffers(1, &fbo_full);
        glDeleteTextures(1, &tex_roi);
    }
    /// Return final texture
    return tex_full;
}
