#pragma once

#include <string>
#include <OpenImageIO/imagebuf.h>

std::string to_string(OIIO::TypeDesc type) {
    switch (type)
    {
    case OIIO::TypeDesc::UNKNOWN: return "UNKNOWN";
    case OIIO::TypeDesc::NONE: return "NONE";
    case OIIO::TypeDesc::UINT8: return "UINT8";
    case OIIO::TypeDesc::INT8: return "INT8";
    case OIIO::TypeDesc::UINT16: return "UINT16";
    case OIIO::TypeDesc::INT16: return "INT16";
    case OIIO::TypeDesc::UINT32: return "UINT32";
    case OIIO::TypeDesc::INT32: return "INT32";
    case OIIO::TypeDesc::UINT64: return "UINT64";
    case OIIO::TypeDesc::INT64: return "INT64";
    case OIIO::TypeDesc::HALF: return "HALF";
    case OIIO::TypeDesc::FLOAT: return "FLOAT";
    case OIIO::TypeDesc::DOUBLE: return "DOUBLE";
    case OIIO::TypeDesc::STRING: return "STRING";
    case OIIO::TypeDesc::PTR: return "PTR";
    case OIIO::TypeDesc::LASTBASE: return "LASTBASE";
    default: return "[Unknown TypeDesc type]";
    }
}



/*
inline std::string replace_framenumber_in_path(const fs::path& path, int framenumber) {
    auto [stem, digits] = split_digits(path.stem().string());
    int digits_count = digits.size();
    // add leading zeros
    std::ostringstream oss;
    oss << std::setw(digits_count) << std::setfill('0') << framenumber;
    digits = oss.str();

    // comose path
    auto layer_path = join_string({
        path.parent_path().string(),
        "/",
        stem,
        digits,
        path.extension().string()
        }, "");
    return layer_path;
}

inline std::string insert_layer_in_path(const fs::path& path, const std::string& layer_name) {
    auto [stem, digits] = split_digits(path.stem().string());
    auto layer_path = join_string({
        path.parent_path().string(),
        "/",
        stem,
        digits,
        digits.empty() ? "" : ".",
        layer_name,
        path.extension().string()
        }, "");
    return layer_path;
}
*/

std::string to_string(OIIO::ImageBuf::IBStorage storage) {
    switch (storage)
    {
    case OpenImageIO_v2_3::ImageBuf::UNINITIALIZED: return "UNINITIALIZED";
    case OpenImageIO_v2_3::ImageBuf::LOCALBUFFER: return "LOCALBUFFER";
    case OpenImageIO_v2_3::ImageBuf::APPBUFFER: return "APPBUFFER";
    case OpenImageIO_v2_3::ImageBuf::IMAGECACHE: return "IMAGECACHE";
    default: return "[Inknown storage type]";
    }

}