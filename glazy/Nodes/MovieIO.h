#include <memory>
#include <regex>

namespace filesystem
{
    class SequencePath
    {
        /// pattern
        ///  
        SequencePath(std::string sequence_pattern, std::tuple<int,int> range)
        {
            static std::regex sequence_match_re("(.+)%0([0-9]+)d(.+)");
            std::smatch sequence_groups;
            if (!std::regex_match(sequence_pattern, sequence_groups, sequence_match_re)) {
                throw "printf pattern not recognized eg.: filename.%04d.exr";
            }
        }

        // create sequence path from item on disk
        static SequencePath from_item_on_disk(std::filesystem::path item)
        {
            assert(std::filesystem::exist(item));
        }

        bool exists() {

        }

        std::filesystem::path at(int frame)
        {
            // thi file not necesseraly exist.
        }

        std::tuple<int, int> range()
        {

        }

        std::vector<int> missing_frames()
        {

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
            cv::Mat mat(spec.width, spec.height, CV_8UC3, data);
            return cap.read(mat);
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