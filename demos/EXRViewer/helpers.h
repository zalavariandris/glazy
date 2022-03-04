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