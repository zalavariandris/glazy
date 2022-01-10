#pragma once

/// keep for reference

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
/** Read the attributes of an exr file */

std::map<std::string, AttributeVariant> read_exr_attributes(std::filesystem::path filename) {
    // open exr
    //
#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
    auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
    auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
    auto file = new Imf::InputFile(*inputStdStream);
#else
    auto file = new Imf::InputFile(filename.c_str());
#endif

    // read openexr header
    // ref: https://github.com/AcademySoftwareFoundation/openexr/blob/master/src/bin/exrheader/main.cpp

    std::map<std::string, AttributeVariant> result;
    auto h = file->header();
    for (auto i = h.begin(); i != h.end(); i++) {
        const Imf::Attribute* a = &i.attribute();
        if (const Imf::StringVectorAttribute* ta = dynamic_cast <const Imf::StringVectorAttribute*> (a))
        {
            std::vector<std::string> value;
            for (const auto val : ta->value()) value.push_back(val);
            result[i.name()] = value;
        }
    }

    // cleanup
#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
    delete file;
    delete inputStdStream;
    delete inputStr;
#else
    delete file;
#endif
    return result;
}