// profile_sequence_display.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "../../tracy/Tracy.hpp"

// from glazy
#include "glazy.h"
#include "pathutils.h"
#include "stringutils.h"

// OpenImageIO
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebuf.h>

// OpenEXR
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfStdIO.h>
#include <OpenEXR/ImfArray.h> //Imf::Array2D
#include <OpenEXR/half.h> // <half> type
#include <OpenEXR/ImfChannelList.h>

// HELPERS
#include "helpers.h"

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

class Sequence
{
public:
    Sequence()
    {
        pattern = "";
        first_frame = 0;
        last_frame = 0;
    }

    Sequence(std::filesystem::path filepath) {
        auto [p, b, e, c] = scan_for_sequence(filepath);
        pattern = p;
        first_frame = b;
        last_frame = e;
    }

    std::filesystem::path item(int F)
    {
        if (pattern.empty()) return "";
        return sequence_item(pattern, F);
    }

    std::filesystem::path pattern;
    int first_frame;
    int last_frame;
};

class DisplayWithOpenExr
{
    GLuint tex{0};
    void* pixels=NULL;
    Imf::Header header;
    Sequence sequence;

public:
    void gui() {

    }

    DisplayWithOpenExr()
    {

    }

    void setup(Sequence seq)
    {
        ZoneScoped;

        sequence = seq;

        Imf::setGlobalThreadCount(8);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void alloc()
    {
        ZoneScoped;
        //std::cout << "alloc" << "\n";
        Imath::Box2i dataWindow = header.dataWindow();
        auto width = dataWindow.max.x - dataWindow.min.x + 1;
        auto height = dataWindow.max.y - dataWindow.min.y + 1;
        pixels = malloc((size_t)width * height * sizeof(half) * 3);
        if(pixels==NULL) {
            std::cerr << "NULL allocation" << "\n";
        }
    }

    void dealloc()
    {
        ZoneScoped;
        free(pixels);
    }

    void update(int F)
    {
        ZoneScoped;

        auto filename = sequence.item(F);

        // open file
        Imf::InputFile* file;

        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
        file = new Imf::InputFile(*inputStdStream);
        #else
        file = new Imf::InputFile(filename.c_str());
        #endif

        /// read header
        header = file->header();


        alloc();


        /// read pixels
        Imath::Box2i dataWindow = header.dataWindow();
        auto width = dataWindow.max.x - dataWindow.min.x + 1;
        auto height = dataWindow.max.y - dataWindow.min.y + 1;
        int dx = dataWindow.min.x;
        int dy = dataWindow.min.y;

        std::vector < std::tuple<int, std::string>> channels{ {0, "B"}, {1, "G"}, {2, "R"} };
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        char* buf = (char*)pixels - dx * sizeof(half) * 3 - dy * (size_t)width * sizeof(half) * 3;
        for (auto [i, name] : channels)
        {
            size_t chanbytes = sizeof(float);
            frameBuffer.insert(name.c_str(),   // name
                Imf::Slice(Imf::PixelType::HALF, // type
                    buf + i * sizeof(half),           // base
                    sizeof(half) * 3,
                    (size_t)width * sizeof(half) * 3
                )
            );
            chanoffset += chanbytes;
        }

        // read pixels
        file->setFrameBuffer(frameBuffer);
        file->readPixels(dataWindow.min.y, dataWindow.max.y);

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, (void*)pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        dealloc();

    }

    void draw()
    {
        ZoneScoped;
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        imdraw::quad(glazy::checkerboard_tex);
        //std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
        imdraw::quad(tex, { -0.9, -0.9 }, { 0.9, 0.9 });
    }

    ~DisplayWithOpenExr() {
        glDeleteTextures(1, &tex);
    }
};

class DisplayWithOpenExr2
{

    Sequence sequence;
    Imf::Header header;
    void* pixels = NULL;
    GLuint pbos[2];
    GLuint tex{ 0 };
    
public:
    void gui() {

    }

    DisplayWithOpenExr2()
    {

    }

    void setup(Sequence seq)
    {
        ZoneScoped;

        sequence = seq;

        Imf::setGlobalThreadCount(8);

        // open file
        auto filename = sequence.item(sequence.first_frame);
        Imf::InputFile* file;

        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
        file = new Imf::InputFile(*inputStdStream);
        #else
        file = new Imf::InputFile(filename.c_str());
        #endif

        /// read header
        header = file->header();

        /// alloc 
        Imath::Box2i displayWindow = header.displayWindow();
        auto width = displayWindow.max.x - displayWindow.min.x + 1;
        auto height = displayWindow.max.y - displayWindow.min.y + 1;
        pixels = malloc((size_t)width * height * sizeof(half) * 3);
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }

        // init texture object
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGB, GL_HALF_FLOAT, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        // init pixel buffers
        glGenBuffers(2, pbos);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[0]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, width*height*3, 0, GL_STREAM_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[1]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 3, 0, GL_STREAM_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void update(int F)
    {
        ZoneScoped;

        auto filename = sequence.item(F);

        // open file
        Imf::InputFile* file;

        #if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__MINGW32__)
        auto inputStr = new std::ifstream(s2ws(filename.string()), std::ios_base::binary);
        auto inputStdStream = new Imf::StdIFStream(*inputStr, filename.string().c_str());
        file = new Imf::InputFile(*inputStdStream);
        #else
        file = new Imf::InputFile(filename.c_str());
        #endif

        /// read header
        header = file->header();

        /// read pixels
        Imath::Box2i dataWindow = header.dataWindow();
        auto width = dataWindow.max.x - dataWindow.min.x + 1;
        auto height = dataWindow.max.y - dataWindow.min.y + 1;
        int dx = dataWindow.min.x;
        int dy = dataWindow.min.y;

        std::vector < std::tuple<int, std::string>> channels{ {0, "B"}, {1, "G"}, {2, "R"} };
        Imf::FrameBuffer frameBuffer;
        size_t chanoffset = 0;
        char* buf = (char*)pixels - dx * sizeof(half) * 3 - dy * (size_t)width * sizeof(half) * 3;
        for (auto [i, name] : channels)
        {
            size_t chanbytes = sizeof(float);
            frameBuffer.insert(name.c_str(),   // name
                Imf::Slice(Imf::PixelType::HALF, // type
                    buf + i * sizeof(half),           // base
                    sizeof(half) * 3,
                    (size_t)width * sizeof(half) * 3
                )
            );
            chanoffset += chanbytes;
        }

        // read pixels
        file->setFrameBuffer(frameBuffer);
        file->readPixels(dataWindow.min.y, dataWindow.max.y);

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, (void*)pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void draw()
    {
        ZoneScoped;
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        imdraw::quad(glazy::checkerboard_tex);
        //std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
        imdraw::quad(tex, {-0.9, -0.9}, {0.9, 0.9});
    }

    ~DisplayWithOpenExr2() {
        glDeleteTextures(1, &tex);
    }
};

class DisplayWithImageInput
{
    GLuint tex;
    void* pixels = NULL;
    OIIO::ImageSpec spec;
    Sequence sequence;

public:
    void gui() {

    }

    DisplayWithImageInput()
    {

    }

    void setup(Sequence seq)
    {
        ZoneScoped;
        sequence = seq;

        OIIO::attribute("threads", 16);
        OIIO::attribute("exr_threads", 16);
        OIIO::attribute("try_all_readers", 0);
        OIIO::attribute("openexr:core", 1);
        if (OIIO::get_int_attribute("openexr:core") == 0) {
            std::cerr << "cant use OpenEXRCore" << "\n";
        }
        else {
            std::cout << "using OpenEXRCore: " << OIIO::get_int_attribute("openexr:core");
        }

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void alloc()
    {
        ZoneScoped;
        pixels = malloc((size_t)spec.width * spec.height * sizeof(half) * 3);
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }
    }

    void dealloc()
    {
        ZoneScoped;
        free(pixels);
    }

    void update(int F)
    {
        ZoneScoped;
        auto filename = sequence.item(F);

        /// read header
        std::unique_ptr<OIIO::ImageInput> in = OIIO::ImageInput::open(filename.string());
        spec = in->spec();

        alloc();

        auto x = spec.x;
        auto y = spec.y;
        auto w = spec.width;
        auto h = spec.height;
        int chbegin = 0;
        int chend = 3;
        in->read_image(chbegin, chend, OIIO::TypeDesc::HALF, pixels);

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_HALF_FLOAT, (void*)pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        dealloc();

    }

    void draw()
    {
        ZoneScoped;
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        imdraw::quad(glazy::checkerboard_tex);
        //std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
        imdraw::quad(tex, { -0.9, -0.9 }, { 0.9, 0.9 });
    }

    ~DisplayWithImageInput() {
        glDeleteTextures(1, &tex);
    }
};

class DisplayWithImageCache
{
    GLuint tex;
    void* pixels = NULL;
    OIIO::ImageSpec spec;
    OIIO::ImageCache* imagecache=NULL;
    Sequence sequence;

public:
    void gui() {

    }

    DisplayWithImageCache()
    {

    }

    void setup(Sequence seq)
    {
        ZoneScoped;
        sequence = seq;

        OIIO::attribute("threads", 16);
        OIIO::attribute("exr_threads", 16);
        OIIO::attribute("try_all_readers", 0);
        OIIO::attribute("openexr:core", 1);
        if (OIIO::get_int_attribute("openexr:core") == 0) {
            std::cerr << "cant use OpenEXRCore" << "\n";
        }
        else {
            std::cout << "using OpenEXRCore: " << OIIO::get_int_attribute("openexr:core");
        }

        imagecache = OIIO::ImageCache::create(true);
        imagecache->attribute("max_memory_MB", 500.0f);
        imagecache->attribute("autotile", 64);
        imagecache->attribute("autoscanline", 1);
        imagecache->attribute("trust_file_extensions ", 1);
        imagecache->attribute("max_memory_MB", 10.0f);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void alloc()
    {
        ZoneScoped;
        pixels = malloc((size_t)spec.width * spec.height * sizeof(half) * 3);
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }
    }

    void dealloc()
    {
        ZoneScoped;
        free(pixels);
    }

    void update(int F)
    {
        ZoneScoped;

        auto filename = sequence.item(F);
        /// read header
        imagecache->get_imagespec(OIIO::ustring(filename.string()), spec, 0, 0);

        alloc();

        /// read pixels
        auto x = spec.x;
        auto y = spec.y;
        auto w = spec.width;
        auto h = spec.height;
        int chbegin = 0;
        int chend = 3;
        imagecache->get_pixels(OIIO::ustring(filename.string()), 0, 0, 
            x, x + w, y, y + h, 0, 1, 
            chbegin, chend, 
            OIIO::TypeDesc::TypeHalf, pixels, 
            OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, 
            chbegin, chend);

        // upload to texture
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_HALF_FLOAT, (void*)pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        dealloc();
    }

    void draw()
    {
        ZoneScoped;
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        imdraw::quad(glazy::checkerboard_tex);
        //std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
        imdraw::quad(tex, { -0.9, -0.9 }, { 0.9, 0.9 });
    }

    ~DisplayWithImageCache() {
        glDeleteTextures(1, &tex);
    }
};

class DisplayWithImageBuffer
{
    GLuint tex;
    void* pixels = NULL;
    OIIO::ImageSpec spec;
    Sequence sequence;
    OIIO::ImageBuf img;

public:
    void gui() {

    }

    DisplayWithImageBuffer()
    {

    }

    void setup(Sequence seq)
    {
        ZoneScoped;
        sequence = seq;

        OIIO::attribute("threads", 16);
        OIIO::attribute("exr_threads", 16);
        OIIO::attribute("try_all_readers", 0);
        OIIO::attribute("openexr:core", 1);
        if (OIIO::get_int_attribute("openexr:core") == 0) {
            std::cerr << "cant use OpenEXRCore" << "\n";
        }
        else {
            std::cout << "using OpenEXRCore: " << OIIO::get_int_attribute("openexr:core");
        }

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void alloc()
    {
        ZoneScoped;
        pixels = malloc((size_t)spec.width * spec.height * sizeof(half) * 3);
        if (pixels == NULL) {
            std::cerr << "NULL allocation" << "\n";
        }
    }

    void dealloc()
    {
        ZoneScoped;
        free(pixels);
    }

    void update(int F)
    {
        ZoneScoped;
        auto filename = sequence.item(F);

        /// read header
        {
            ZoneScopedN("open file");
            img = OIIO::ImageBuf(filename.string());
        }
        {
            ZoneScopedN("read spec");
            spec = img.spec();
        }

        alloc();

        // read pixels
        int x, y, w, h;
        {
            ZoneScopedN("get pixels");
            x = spec.x;
            y = spec.y;
            w = spec.width;
            h = spec.height;
            int chbegin = 0;
            int chend = 3;
            auto roi = OIIO::ROI(x, x + w, y, y + h, 0, 1, chbegin, chend);
            img.get_pixels(roi, OIIO::TypeDesc::TypeHalf, pixels);
        }

        // to texture
        {
            ZoneScopedN("to texture");
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_HALF_FLOAT, (void*)pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        dealloc();
    }

    void draw()
    {
        ZoneScoped;
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        imdraw::quad(glazy::checkerboard_tex);
        //std::cout << "draw from: " << texIds[texCurrentIndex].id << "\n";
        imdraw::quad(tex, { -0.9, -0.9 }, { 0.9, 0.9 });
    }

    ~DisplayWithImageBuffer(){
        glDeleteTextures(1, &tex);
    }
};


int main()
{
    //WithImageInput::ProfileSequenceDisplay();
    //WitOpenEXR::ProfileSequenceDisplay();

    glazy::init();
    glfwSwapInterval(0);
    std::filesystem::path path{ "C:/Users/andris/Desktop/52_06_EXAM-half/52_06_EXAM_v04-vrayraw.0005.exr" };
    //std::filesystem::path path{ "R:/PlasticSky/Render/52_EXAM_v51/52_01_EXAM_v51.0001.exr" };
    //std::filesystem::path path{ "C:/Users/andris/Desktop/52_06_EXAM_v51-raw/52_06_EXAM_v51.0001.exr" };
    //std::filesystem::path path{ "C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr" };
    Sequence sequence(path);
    int F = sequence.first_frame;
    {
        DisplayWithOpenExr disp_exr;
        disp_exr.setup(path);
        DisplayWithOpenExr2 disp_exr2;
        disp_exr2.setup(path);
        DisplayWithImageCache disp_imagecache;
        disp_imagecache.setup(path);
        DisplayWithImageInput disp_imageinput;
        disp_imageinput.setup(path);
        DisplayWithImageBuffer disp_imagebuffer;
        disp_imagebuffer.setup(path);
        while (glazy::is_running()) {
            glazy::new_frame();
            disp_imagebuffer.update(F);
            disp_imagebuffer.draw();
            glazy::end_frame();
            FrameMark;
            F++;
            if (F > sequence.last_frame) F = sequence.first_frame;
        }
    }
    glazy::destroy();
}