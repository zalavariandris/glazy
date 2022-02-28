#pragma once
#include <string>
#include <glad/glad.h>

std::string to_string(GLint t) {
    switch (t) {
        case GL_RGB8: return "GL_RGB8";
        case GL_RGB16F: return "GL_RGB16F";
        case GL_RGB32F: return "GL_RGB32F";
        case GL_RGBA8: return "GL_RGBA8";
        case GL_RGBA16F: return "GL_RGBA16F";
        case GL_RGBA32F: return "GL_RGBA32F";

        case GL_RED: return "GL_RED";
        case GL_GREEN: return "GL_GREEN";
        case GL_BLUE: return "GL_BLUE";
        case GL_ALPHA: return "GL_ALPHA";
        case GL_ZERO: return "GL_ZERO";
        case GL_ONE: return "GL_ONE";

        default: return "[UNKNOWN GLint]";
    }
}

std::string to_string(GLenum t)
{
    switch (t)
    {
        // GLtypes
        case GL_UNSIGNED_INT: return "GL_UNSIGNED_INT";
        case GL_FLOAT: return "GL_FLOAT";
        case GL_HALF_FLOAT: return "GL_HALF_FLOAT";

        // Base Internal Format
        case GL_RGB:  return "GL_RGB";
        case GL_BGR:  return "GL_BGR";
        case GL_RGBA: return "GL_RGBA";
        case GL_BGRA: return "GL_BGRA";
        case GL_RED: return "GL_RED";
        case GL_RG: return "GL_RG";

        // Sized Internal Format
        case GL_RGB16F:  return "GL_RGB16F";
        case GL_RGBA16F: return "GL_RGBA16F";
        case GL_RGB32F:  return "GL_RGB32F";
        case GL_RGBA32F: return "GL_RGBA32F";
        
        // return unknown value
        default: return "[UNKNOWN GLenum]";
    }
}