#pragma once

#include <string>
#include <OpenEXR/ImfPixelType.h>
#include <glad/glad.h>

std::string to_string(Imf::PixelType t) {
    switch (t)
    {
    case Imf::UINT:
        return "Imf::UINT";
    case Imf::HALF:
        return "Imf::HALF";
    case Imf::FLOAT:
        return "Imf::FLOAT";
    default:
        return "[UNKNOWN]";
        break;
    }
}

//
// Return the size of a single value of the indicated type,
// in the machine's native format.
//
int pixelTypeSize(Imf::PixelType t)
{
    int size;

    switch (t)
    {
    case Imf::UINT:

        size = sizeof(unsigned int);
        break;

    case Imf::HALF:

        size = sizeof(half);
        break;

    case Imf::FLOAT:

        size = sizeof(float);
        break;

    default:

        throw IEX_NAMESPACE::ArgExc("Unknown pixel type.");
    }

    return size;
}

/// Get opengl format for OIIO spec
/// internalformat, format, type</returns>
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
