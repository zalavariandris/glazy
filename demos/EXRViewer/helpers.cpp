//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#include <OpenExr/ImfBoxAttribute.h>
#include <OpenExr/ImfChannelListAttribute.h>
#include <OpenExr/ImfChromaticitiesAttribute.h>
#include <OpenExr/ImfCompressionAttribute.h>
#include <OpenExr/ImfDoubleAttribute.h>
#include <OpenExr/ImfEnvmapAttribute.h>
#include <OpenExr/ImfFloatAttribute.h>
#include <OpenExr/ImfHeader.h>
#include <OpenExr/ImfIntAttribute.h>
#include <OpenExr/ImfKeyCodeAttribute.h>
#include <OpenExr/ImfLineOrderAttribute.h>
#include <OpenExr/ImfMatrixAttribute.h>
#include <OpenExr/ImfMultiPartInputFile.h>
#include <OpenExr/ImfPreviewImageAttribute.h>
#include <OpenExr/ImfRationalAttribute.h>
#include <OpenExr/ImfStringAttribute.h>
#include <OpenExr/ImfStringVectorAttribute.h>
#include <OpenExr/ImfTileDescriptionAttribute.h>
#include <OpenExr/ImfTimeCodeAttribute.h>
#include <OpenExr/ImfVecAttribute.h>
#include <OpenExr/ImfVersion.h>

#include <iomanip>
#include <iostream>
#include <sstream>

using namespace Imf;
using namespace std;

std::string printCompression(Compression c)
{
    switch (c)
    {
    case NO_COMPRESSION: return "none"; break;

    case RLE_COMPRESSION: return "run-length encoding"; break;

    case ZIPS_COMPRESSION:return "zip, individual scanlines"; break;

    case ZIP_COMPRESSION:return "zip, multi-scanline blocks"; break;

    case PIZ_COMPRESSION: return "piz"; break;

    case PXR24_COMPRESSION: return "pxr24"; break;

    case B44_COMPRESSION: return "b44"; break;

    case B44A_COMPRESSION: return "b44a"; break;

    case DWAA_COMPRESSION: return "dwa, small scanline blocks"; break;

    case DWAB_COMPRESSION: return "dwa, medium scanline blocks"; break;

    default: return std::to_string( int(c) ); break;
    }
}

std::string printLineOrder(LineOrder lo)
{
    switch (lo)
    {
    case INCREASING_Y: return "increasing y"; break;

    case DECREASING_Y: return "decreasing y"; break;

    case RANDOM_Y: return "random y"; break;

    default: return std::to_string( int(lo) ); break;
    }
}

std::string printPixelType(PixelType pt)
{
    switch (pt)
    {
    case UINT: return "32-bit unsigned integer"; break;

    case HALF: return "16-bit floating-point"; break;

    case FLOAT: return "32-bit floating-point"; break;

    default: return "type " + std::to_string( int(pt) ); break;
    }
}

std::string printLevelMode(LevelMode lm)
{
    switch (lm)
    {
    case ONE_LEVEL: return "single level"; break;

    case MIPMAP_LEVELS: return "mip-map"; break;

    case RIPMAP_LEVELS: return "rip-map"; break;

    default: return "level mode " + std::to_string( int(lm) ); break;
    }
}

std::string
printLevelRoundingMode(LevelRoundingMode lm)
{
    switch (lm)
    {
    case ROUND_DOWN: return "down"; break;

    case ROUND_UP: return "up"; break;

    default: return "mode " + std::to_string( int(lm) ); break;
    }
}

std::string
printTimeCode(TimeCode tc)
{
    std::stringstream ss;
    ss << "    "
        "time "
        << setfill('0') <<
#ifndef HAVE_COMPLETE_IOMANIP
        setw(2) << tc.hours() << ":" << setw(2) << tc.minutes() << ":"
        << setw(2) << tc.seconds() << ":" << setw(2) << tc.frame() << "\n"
        <<
#else
        setw(2) << right << tc.hours() << ":" << setw(2) << right
        << tc.minutes() << ":" << setw(2) << right << tc.seconds() << ":"
        << setw(2) << right << tc.frame() << "\n"
        <<
#endif
        setfill(' ')
        << "    "
        "drop frame "
        << tc.dropFrame()
        << ", "
        "color frame "
        << tc.colorFrame()
        << ", "
        "field/phase "
        << tc.fieldPhase()
        << "\n"
        "    "
        "bgf0 "
        << tc.bgf0()
        << ", "
        "bgf1 "
        << tc.bgf1()
        << ", "
        "bgf2 "
        << tc.bgf2()
        << "\n"
        "    "
        "user data 0x"
        << hex << tc.userData() << dec;
    return ss.str();
}

std::string 
printEnvmap(const Envmap& e)
{
    switch (e)
    {
    case ENVMAP_LATLONG: return "latitude-longitude map"; break;

    case ENVMAP_CUBE: return "cube-face map"; break;

    default: return "map type " + std::to_string( int(e) ); break;
    }
}

std::string
printChannelList(const ChannelList& cl)
{
    std::stringstream ss;
    for (ChannelList::ConstIterator i = cl.begin(); i != cl.end(); ++i)
    {
        ss << "\n    " << i.name() << ", "<< printPixelType(i.channel().type);

        ss << ", sampling " << i.channel().xSampling << " "
            << i.channel().ySampling;

        if (i.channel().pLinear) ss << ", plinear";
    }
    return ss.str();
}

std::string
printInfo(const MultiPartInputFile& in)
{
    std::stringstream ss;
    int                parts = in.parts();

    //
    // Check to see if any parts are incomplete
    //

    bool fileComplete = true;

    for (int i = 0; i < parts && fileComplete; ++i)
        if (!in.partComplete(i)) fileComplete = false;

    //
    // Print file format version
    //

    ss << "file format version: " << getVersion(in.version())
        << ", "
        "flags 0x"
        << setbase(16) << getFlags(in.version()) << setbase(10) << "\n";

    //
    // Print the header of every part in the file
    //

    for (int p = 0; p < parts; ++p)
    {
        const Header& h = in.header(p);

        if (parts != 1)
        {
            ss << "\n\n part " << p
                << (in.partComplete(p) ? "" : " (incomplete)") << ":\n";
        }

        for (Header::ConstIterator i = h.begin(); i != h.end(); ++i)
        {
            const Attribute* a = &i.attribute();
            ss << i.name() << " (type " << a->typeName() << ")";

            if (const Box2iAttribute* ta =
                dynamic_cast<const Box2iAttribute*> (a))
            {
                ss << ": " << ta->value().min << " - " << ta->value().max;
            }

            else if (
                const Box2fAttribute* ta =
                dynamic_cast<const Box2fAttribute*> (a))
            {
                ss << ": " << ta->value().min << " - " << ta->value().max;
            }
            else if (
                const ChannelListAttribute* ta =
                dynamic_cast<const ChannelListAttribute*> (a))
            {
                ss << ":" << printChannelList(ta->value());
            }
            else if (
                const ChromaticitiesAttribute* ta =
                dynamic_cast<const ChromaticitiesAttribute*> (a))
            {
                ss << ":\n"
                    "    red   "
                    << ta->value().red
                    << "\n"
                    "    green "
                    << ta->value().green
                    << "\n"
                    "    blue  "
                    << ta->value().blue
                    << "\n"
                    "    white "
                    << ta->value().white;
            }
            else if (
                const CompressionAttribute* ta =
                dynamic_cast<const CompressionAttribute*> (a))
            {
                ss << ": " << printCompression(ta->value());
            }
            else if (
                const DoubleAttribute* ta =
                dynamic_cast<const DoubleAttribute*> (a))
            {
                ss << ": " << ta->value();
            }
            else if (
                const EnvmapAttribute* ta =
                dynamic_cast<const EnvmapAttribute*> (a))
            {
                ss << ": " << printEnvmap(ta->value());
            }
            else if (
                const FloatAttribute* ta =
                dynamic_cast<const FloatAttribute*> (a))
            {
                ss << ": " << ta->value();
            }
            else if (
                const IntAttribute* ta = dynamic_cast<const IntAttribute*> (a))
            {
                ss << ": " << ta->value();
            }
            else if (
                const KeyCodeAttribute* ta =
                dynamic_cast<const KeyCodeAttribute*> (a))
            {
                ss << ":\n"
                    "    film manufacturer code "
                    << ta->value().filmMfcCode()
                    << "\n"
                    "    film type code "
                    << ta->value().filmType()
                    << "\n"
                    "    prefix "
                    << ta->value().prefix()
                    << "\n"
                    "    count "
                    << ta->value().count()
                    << "\n"
                    "    perf offset "
                    << ta->value().perfOffset()
                    << "\n"
                    "    perfs per frame "
                    << ta->value().perfsPerFrame()
                    << "\n"
                    "    perfs per count "
                    << ta->value().perfsPerCount();
            }
            else if (
                const LineOrderAttribute* ta =
                dynamic_cast<const LineOrderAttribute*> (a))
            {
                ss << ": " << printLineOrder(ta->value());
            }
            else if (
                const M33fAttribute* ta =
                dynamic_cast<const M33fAttribute*> (a))
            {
                ss << ":\n"
                    "   ("
                    << ta->value()[0][0] << " " << ta->value()[0][1] << " "
                    << ta->value()[0][2] << "\n    " << ta->value()[1][0]
                    << " " << ta->value()[1][1] << " " << ta->value()[1][2]
                    << "\n    " << ta->value()[2][0] << " "
                    << ta->value()[2][1] << " " << ta->value()[2][2] << ")";
            }
            else if (
                const M44fAttribute* ta =
                dynamic_cast<const M44fAttribute*> (a))
            {
                ss << ":\n"
                    "   ("
                    << ta->value()[0][0] << " " << ta->value()[0][1] << " "
                    << ta->value()[0][2] << " " << ta->value()[0][3]
                    << "\n    " << ta->value()[1][0] << " "
                    << ta->value()[1][1] << " " << ta->value()[1][2] << " "
                    << ta->value()[1][3] << "\n    " << ta->value()[2][0]
                    << " " << ta->value()[2][1] << " " << ta->value()[2][2]
                    << " " << ta->value()[2][3] << "\n    "
                    << ta->value()[3][0] << " " << ta->value()[3][1] << " "
                    << ta->value()[3][2] << " " << ta->value()[3][3] << ")";
            }
            else if (
                const PreviewImageAttribute* ta =
                dynamic_cast<const PreviewImageAttribute*> (a))
            {
                ss << ": " << ta->value().width() << " by "
                    << ta->value().height() << " pixels";
            }
            else if (
                const StringAttribute* ta =
                dynamic_cast<const StringAttribute*> (a))
            {
                ss << ": \"" << ta->value() << "\"";
            }
            else if (
                const StringVectorAttribute* ta =
                dynamic_cast<const StringVectorAttribute*> (a))
            {
                ss << ":";

                for (StringVector::const_iterator i = ta->value().begin();
                    i != ta->value().end();
                    ++i)
                {
                    ss << "\n    \"" << *i << "\"";
                }
            }
            else if (
                const RationalAttribute* ta =
                dynamic_cast<const RationalAttribute*> (a))
            {
                ss << ": " << ta->value().n << "/" << ta->value().d << " ("
                    << double(ta->value()) << ")";
            }
            else if (
                const TileDescriptionAttribute* ta =
                dynamic_cast<const TileDescriptionAttribute*> (a))
            {
                ss << ":\n    ";

                ss << printLevelMode(ta->value().mode);

                ss << "\n    tile size " << ta->value().xSize << " by "
                    << ta->value().ySize << " pixels";

                if (ta->value().mode != ONE_LEVEL)
                {
                    ss << "\n    level sizes rounded ";
                    ss << printLevelRoundingMode(ta->value().roundingMode);
                }
            }
            else if (
                const TimeCodeAttribute* ta =
                dynamic_cast<const TimeCodeAttribute*> (a))
            {
                ss << ":\n";
                ss << printTimeCode(ta->value());
            }
            else if (
                const V2iAttribute* ta = dynamic_cast<const V2iAttribute*> (a))
            {
                ss << ": " << ta->value();
            }
            else if (
                const V2fAttribute* ta = dynamic_cast<const V2fAttribute*> (a))
            {
                ss << ": " << ta->value();
            }
            else if (
                const V3iAttribute* ta = dynamic_cast<const V3iAttribute*> (a))
            {
                ss << ": " << ta->value();
            }
            else if (
                const V3fAttribute* ta = dynamic_cast<const V3fAttribute*> (a))
            {
                ss << ": " << ta->value();
            }

            ss << '\n';
        }
    }

    ss << endl;
    return ss.str();
}