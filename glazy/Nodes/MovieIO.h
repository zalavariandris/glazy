#include <memory>
#include <regex>

namespace filesystem
{
    class SequencePath
    {
    private:
        std::string _pattern;
        int _first_frame;
        int _last_frame;
    public:
        /// pattern
        ///  
        SequencePath(std::string sequence_pattern, std::tuple<int,int> range)
        {
            static std::regex sequence_match_re("(.+)%0([0-9]+)d(.+)");
            std::smatch sequence_groups;
            if (!std::regex_match(sequence_pattern, sequence_groups, sequence_match_re)) {
                throw "printf pattern not recognized eg.: filename.%04d.exr";
            }

            _pattern = sequence_pattern;
            _first_frame = std::get<0>(range);
            _last_frame = std::get<1>(range);
        }

        // create sequence path from item on disk
        static SequencePath from_item_on_disk(std::filesystem::path item)
        {
            assert(std::filesystem::exists(item));
        }

        bool exists() {
            auto first_path = at(_first_frame);
            auto last_path = at(_last_frame);

            return std::filesystem::exists(first_path) && std::filesystem::exists(last_path);
        }

        std::filesystem::path at(int frame)
        {
            /// this file does not necesseraly exists.

            // ref: educative.io educative.io/edpresso/how-to-use-the-sprintf-method-in-c
            int size_s = std::snprintf(nullptr, 0, _pattern.c_str(), frame) + 1; // Extra space for '\0'
            if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
            auto size = static_cast<size_t>(size_s);
            auto buf = std::make_unique<char[]>(size);
            std::snprintf(buf.get(), size, _pattern.c_str(), frame);

            auto filename = std::string(buf.get(), buf.get() + size - 1);
            return std::filesystem::path(filename);
        }

        std::tuple<int, int> range()
        {
            return { _first_frame, _last_frame };
        }

        std::vector<int> missing_frames()
        {
            std::vector<int> missing;
            for (int frame = _first_frame; frame <= _last_frame; frame++)
            {
                if (!std::filesystem::exists(at(frame)))
                {
                    missing.push_back(frame);
                }
            }
            return missing;
        }
    };
}

namespace MovieIO {
    struct Info {
        int x;
        int y;
        int width;
        int height;
        int nchannels;
        OIIO::TypeDesc format;
    };

    class MovieInput {
    public:
        static std::unique_ptr<MovieInput> open(const std::string& name);

        virtual std::tuple<int, int> range() const = 0;

        virtual void seek(int frame) = 0;

        virtual Info info() const = 0;

        virtual bool read(void* data) = 0;
    };

    class OIIOMovieInput : public MovieInput
    {
        std::unique_ptr<OIIO::ImageInput> _current_input;
        std::filesystem::path _current_filepath;
        std::string _sequence_path;
        bool _is_sequence { false };
        int _current_frame;
    public:
        int _first_frame;
        int _last_frame;

        // accepts an existing image file, or printf pattern file sequence
        // eg folder/filename.0007.exr -> single frame
        //    folder/filename.%04d.exr -> sequence of images on disk
        OIIOMovieInput(std::string sequence_path)
        {  
            std::string filename = std::filesystem::path(sequence_path).filename().string();

            std::string prefix;
            std::string digits;
            std::string affix;
            bool IsSequencePattern = false;
            // input is printf pattern eg.: folder/file.%04d.exr
            {
                /// Parse sequence format
                /// Match input
                /// (filename).%0(4)d(.exr)
                static std::regex sequence_match_re("(.+)%0([0-9]+)d(.+)");
                std::smatch sequence_groups;
                IsSequencePattern = std::regex_match(filename, sequence_groups, sequence_match_re);

                prefix = sequence_groups[1];
                digits = sequence_groups[2];
                affix = std::string(sequence_groups[3]);
            }

            if (IsSequencePattern)
            {
                /// Find sequence items on disk
                auto input_folder = std::filesystem::path(sequence_path).parent_path();
                if (!std::filesystem::exists(input_folder))
                    throw std::filesystem::filesystem_error("No sequence", input_folder, std::error_code(2, std::generic_category()));

                std::string re_str = R"(([0-9]{)";
                std::string item_match_str = prefix + (re_str + digits + "})") + affix;
                std::regex item_match_re(item_match_str);
                std::vector<std::filesystem::path> sequence;
                std::vector<int> framenumbers;
                for (auto&& entry : std::filesystem::directory_iterator(input_folder, std::filesystem::directory_options::skip_permission_denied))
                {
                    std::string item_name = entry.path().filename().string();
                    std::smatch item_groups;
                    bool IsSequenceItem = std::regex_match(item_name, item_groups, item_match_re);

                    if (IsSequenceItem) {
                        sequence.push_back(entry.path());
                        framenumbers.push_back(stoi(item_groups[1]));
                    }
                }

                // if no sequence items found, throw an error
                if (framenumbers.empty()) throw std::filesystem::filesystem_error("No sequence with pattern", sequence_path, std::error_code(2, std::generic_category()));

                // get first and last frames
                std::sort(framenumbers.begin(), framenumbers.end());

                _is_sequence = true;
                _sequence_path = sequence_path;
                _first_frame = framenumbers[0];
                _last_frame = framenumbers.back();
            }
            else if(std::filesystem::exists(sequence_path))
            {
                _is_sequence = false;
                _sequence_path = sequence_path;
                _first_frame = 0;
                _last_frame = 0;
            }

            _sequence_path = sequence_path;
            _current_frame = _first_frame;
            _current_filepath = sequence_item(sequence_path, _current_frame);
            _current_input = OIIO::ImageInput::open(_current_filepath.string());
        }

        std::tuple<int, int> range() const
        {
            return { _first_frame, _last_frame };
        }

        void seek(int frame) override
        {
            if (_current_frame == frame) return;

            _current_frame = frame;
            if (_is_sequence) {
                _current_filepath = sequence_item(_sequence_path, _current_frame);
                _current_input = OIIO::ImageInput::open(_current_filepath.string());
            }
        }

        Info info() const override
        {
            auto spec = _current_input->spec();
            return {
                spec.x,spec.y,spec.width,spec.height,
                4,
                OIIO::TypeHalf
            };
        }

        bool read(void* data) override
        {
            return _current_input->read_image(0, 4, OIIO::TypeHalf, data);
        }
    };

    std::string cvtype2str(int type) {
        std::string r;

        uchar depth = type & CV_MAT_DEPTH_MASK;
        uchar chans = 1 + (type >> CV_CN_SHIFT);

        switch (depth) {
        case CV_8U:  r = "8U"; break;
        case CV_8S:  r = "8S"; break;
        case CV_16U: r = "16U"; break;
        case CV_16S: r = "16S"; break;
        case CV_32S: r = "32S"; break;
        case CV_32F: r = "32F"; break;
        case CV_64F: r = "64F"; break;
        default:     r = "User"; break;
        }

        r += "C";
        r += (chans + '0');

        return r;
    }

    class OpenCVMovieInput : public MovieInput
    {
    private:
        cv::VideoCapture cap;
        int _first_frame;
        int _last_frame;
    public:
        OpenCVMovieInput(std::string name)
        {
            cap = cv::VideoCapture(name);

            //cv::Mat image;
            //cap.read(image);
            //auto tname = cvtype2str(image.type());

            _first_frame = 0;
            _last_frame = cap.get(cv::CAP_PROP_FRAME_COUNT);
        }

        std::tuple<int, int> range() const
        {
            return { _first_frame, _last_frame };
        }

        void seek(int frame) override
        {
            if (cap.get(cv::CAP_PROP_POS_FRAMES) != frame)
            {
                cap.set(cv::CAP_PROP_POS_FRAMES, frame);
            }
        }

        Info info() const override
        {
            return {
                0,0,
                (int)cap.get(cv::CAP_PROP_FRAME_WIDTH),
                (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT),
                3,
                OIIO::TypeUInt8
            };
        }
        
        bool read(void* data) override
        {
            auto spec = info();
            
            cv::Mat mat(spec.height, spec.width, CV_8UC3, data);
            bool success = cap.read(mat);
            //cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
            return success;
        }
    };

    // Factory
    std::unique_ptr<MovieInput> MovieInput::open(const std::string& name)
    {
        auto ext = std::filesystem::path(name).extension();
        if (ext == ".mp4")
        {
            return std::make_unique<OpenCVMovieInput>(name);
        }
        else if(ext==".exr")
        {
            return std::make_unique<OIIOMovieInput>(name);
        }
    }
}