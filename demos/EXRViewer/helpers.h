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

#pragma region RenderToTexture
std::vector<GLuint> fbo_stack;
std::vector<std::array<GLint, 4>> viewport_stack;

// begin rendering to fbo, at glviewport: x,y,width,height
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

std::string get_infostring(const Imf::MultiPartInputFile& in);

std::string to_string(Imf::PixelType pt)
{
    switch (pt)
    {
    case Imf::UINT: return "32-bit unsigned integer"; break;

    case Imf::HALF: return "16-bit floating-point"; break;

    case Imf::FLOAT: return "32-bit floating-point"; break;

    default: return "type " + std::to_string(int(pt)); break;
    }
}