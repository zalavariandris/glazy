#pragma once

//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

//-----------------------------------------------------------------------------
//
//	Utility program to print an image file's header
//
//-----------------------------------------------------------------------------

#include <OpenExr/ImfMultiPartInputFile.h>
#include <string>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/half.h> // <half> type

#include <vector>
#include <array>

#pragma region RenderToTexture
static std::vector<GLuint> fbo_stack;
static std::vector<std::array<GLint, 4>> viewport_stack;

// begin rendering to fbo, at glviewport: x,y,width,height
inline void BeginRenderToTexture(GLuint fbo, GLint x, GLint y, GLsizei width, GLsizei height)
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

inline void EndRenderToTexture()
{
    assert(("Mismatch Begin/End Rendertargets", !fbo_stack.empty()));

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

//#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
//#include <Windows.h>
//inline std::wstring s2ws(const std::string& s)
//{
//    int len;
//    int slength = (int)s.length() + 1;
//
//    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
//    wchar_t* buf = new wchar_t[len];
//    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
//    std::wstring r(buf);
//    delete[] buf;
//
//    return r;
//}
//#endif

inline GLenum glformat_from_channels(std::vector<std::string> channels, std::array<GLint, 4>& swizzle_mask)
{
    /// Match glformat and swizzle to data
    /// this is a very important step. It depends on the framebuffer channel order.
    /// exr order channel names in aplhebetic order. So by default, ABGR is the read order.
    /// upstream this can be changed, therefore we must handle various channel orders eg.: RGBA, BGRA, and alphebetically sorted eg.: ABGR channel orders.

    GLenum glformat;
    if (channels == std::vector<std::string>({ "B", "G", "R", "A" }))
    {
        glformat = GL_BGRA;
        swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    }
    else if (channels == std::vector<std::string>({ "R", "G", "B", "A" }))
    {
        glformat = GL_BGRA;
        swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    }
    else if (channels == std::vector<std::string>({ "A", "B", "G", "R" }))
    {
        glformat = GL_BGRA;
        swizzle_mask = { GL_ALPHA, GL_RED, GL_GREEN, GL_BLUE };
    }
    else if (channels == std::vector<std::string>({ "B", "G", "R" }))
    {
        glformat = GL_BGR;
        swizzle_mask = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
    }
    else if (channels.size() == 4)
    {
        glformat = GL_BGRA;
        swizzle_mask = { GL_RED, GL_GREEN, GL_BLUE , GL_ALPHA };
    }
    else if (channels.size() == 3)
    {
        glformat = GL_RGB;
        swizzle_mask = { GL_RED , GL_GREEN, GL_BLUE, GL_ONE };
    }
    else if (channels.size() == 2) {
        glformat = GL_RG;
        swizzle_mask = { GL_RED, GL_GREEN, GL_ZERO, GL_ONE };
    }
    else if (channels.size() == 1)
    {
        glformat = GL_RED;
        if (channels[0] == "R") swizzle_mask = { GL_RED, GL_ZERO, GL_ZERO, GL_ONE };
        else if (channels[0] == "G") swizzle_mask = { GL_ZERO, GL_RED, GL_ZERO, GL_ONE };
        else if (channels[0] == "B") swizzle_mask = { GL_ZERO, GL_ZERO, GL_RED, GL_ONE };
        else swizzle_mask = { GL_RED, GL_RED, GL_RED, GL_ONE };
    }
    else {
        std::cout << "cannot match selected channels to glformat: ";
        for (auto channel : channels) {
            std::cout << channel << " ";
        }
        std::cout << "\n";
        return -1;
    }
    return glformat;
}

inline int pixelTypeSize(Imf::PixelType type)
{
    int size;

    switch (type)
    {
    case OPENEXR_IMF_INTERNAL_NAMESPACE::UINT:

        size = sizeof(unsigned int);
        break;

    case OPENEXR_IMF_INTERNAL_NAMESPACE::HALF:

        size = sizeof(half);
        break;

    case OPENEXR_IMF_INTERNAL_NAMESPACE::FLOAT:

        size = sizeof(float);
        break;

    default: throw IEX_NAMESPACE::ArgExc("Unknown pixel type.");
    }

    return size;
}

inline std::string get_infostring(const Imf::MultiPartInputFile& in);

inline std::string to_string(Imf::PixelType pt)
{
    switch (pt)
    {
    case Imf::UINT: return "32-bit unsigned integer"; break;

    case Imf::HALF: return "16-bit floating-point"; break;

    case Imf::FLOAT: return "32-bit floating-point"; break;

    default: return "type " + std::to_string(int(pt)); break;
    }
}