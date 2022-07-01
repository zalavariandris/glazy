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

inline std::tuple<std::string, std::string> split_channel_id(const std::string& channel_id)
{
    // find the last delimiter positions
    size_t found = channel_id.find_last_of(".");
    // if no delimiter, than viewlayer is an empty string
    // othervise its the part before the last delimiter
    auto viewlayer = (found != std::string::npos) ? channel_id.substr(0, found) : "";

    auto channel = channel_id.substr(found + 1);
    return { viewlayer, channel };
}

inline std::vector<std::tuple<int, int>> group_stringvector_by_patterns(const std::vector<std::string>& statement, const std::vector<std::vector<std::string>>& patterns)
{
    std::vector<std::tuple<int, int>> groups;

    // sort patterns
    auto sorted_patterns = patterns;
    std::sort(sorted_patterns.begin(), sorted_patterns.end(), [](const std::vector<std::string>& A, const std::vector<std::string>& B) {
        return A.size() > B.size();
        });

    // collect pattern matches
    int i = 0;
    while (i < statement.size())
    {
        std::optional<std::tuple<int, int>> match;

        // check all patterns at current index
        // if match is found, move iterator to the end of match
        for (std::vector<std::string> pattern : sorted_patterns) {
            auto begin = i;
            auto end = i + pattern.size();
            auto statement_sample = std::vector<std::string>(statement.begin() + begin, statement.begin() + std::min(end, statement.size()));
            bool IsMatch = pattern == statement_sample;
            if (IsMatch)
            {
                match = std::tuple<int, int>(begin, end);
                i = end - 1;
                break;
            }
        }

        // if match is found, add it to the stack
        // otherwise, keep single item 
        if (match)
        {
            groups.push_back(match.value());
        }
        else {
            groups.push_back(std::tuple<int, int>(i, i + 1));
        }
        i++;
    }
    return groups;
}


namespace MovieIO {
    struct Info {
        int x;
        int y;
        int width;
        int height;
        std::vector<std::string> channels;
        OIIO::TypeDesc format;
    };

    struct ChannelSet {
        std::string name;
        int part;
        std::vector<std::string> channels;

        ChannelSet() :name("color"), part(0), channels({ "R", "G", "B", "A" }){};

        ChannelSet(std::string name, int part, std::vector<std::string> channels) : 
            name(name), part(part), channels(channels){}
    };

    class MovieInput {
    public:
        static std::unique_ptr<MovieInput> open(const std::string& name);

        virtual std::tuple<int, int> range() const = 0;

        virtual bool seek(int frame) = 0;

        virtual Info info() const = 0;

        virtual bool read(void* data, ChannelSet channel_set=ChannelSet()) = 0;

        virtual std::vector<ChannelSet> channel_sets() = 0;
    };

    class OIIOMovieInput : public MovieInput
    {
        std::unique_ptr<OIIO::ImageInput> _current_input;
        std::filesystem::path _current_filepath;
        std::string _sequence_path;
        bool _is_sequence { false };
        int _current_frame;
        std::vector<ChannelSet> _channel_sets;
    public:
        int _first_frame;
        int _last_frame;

        // accepts an existing image file, or printf pattern file sequence
        // eg folder/filename.0007.exr -> single frame
        //    folder/filename.%04d.exr -> sequence of images on disk
        OIIOMovieInput(std::string sequence_path)
        {
            ///
            /// Parse input sequence pattern
            /// 

            std::string filename = std::filesystem::path(sequence_path).filename().string();

            std::string prefix;
            std::string digits;
            std::string affix;

            const std::regex sequence_match_re("(.+)%0([0-9]+)d(.+)");
            std::smatch sequence_groups;
            if (std::regex_match(filename, sequence_groups, sequence_match_re))
            {
                // case 1.: sequence with printf format eg.: folder/filename.%04d.exr
                prefix = sequence_groups[1];
                digits = sequence_groups[2];
                affix = std::string(sequence_groups[3]);

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
                // case 1.: single image
                _is_sequence = false;
                _sequence_path = sequence_path;
                _first_frame = 0;
                _last_frame = 0;
            }
            else {
                throw std::filesystem::filesystem_error("File or sequence does not exist!", sequence_path, std::error_code(2, std::generic_category()));
            }

            _sequence_path = sequence_path;
            _current_frame = _first_frame;
            _current_filepath = sequence_item(sequence_path, _current_frame);
            _current_input = OIIO::ImageInput::open(_current_filepath.string());

            update_channel_sets();
        }

        void update_channel_sets() {
            /// 
            /// Prepare channel sets
            /// 

            /// Collect layers per subimage
            {
                std::vector<ChannelSet> part_channel_sets;
                int subimage = 0;
                while (_current_input->seek_subimage(subimage, 0)) {
                    const auto& spec = _current_input->spec();
                    std::vector<std::string> channels;
                    for (int channel = 0; channel < spec.nchannels; channel++)
                    {
                        std::string channel_name = spec.channel_name(channel);
                        channels.push_back(channel_name);
                    }
                    auto name = spec.get_string_attribute("name");
                    auto layer = ChannelSet(name, subimage, channels);
                    layer.part = subimage;

                    subimage++;

                    part_channel_sets.push_back(layer);
                }
                this->_channel_sets = part_channel_sets;
            }

            /// group layers by delimiter
            {
                std::vector<ChannelSet> layers_by_delimiter;
                for (auto& layer : this->_channel_sets)
                {
                    std::vector<ChannelSet> child_layers;
                    for (const auto& channel : layer.channels)
                    {
                        size_t found = channel.find_last_of(".");
                        auto viewlayer = (found != std::string::npos) ? channel.substr(0, found) : "";
                        auto channel_name = channel.substr(found + 1);

                        if (child_layers.empty() || child_layers.back().name != viewlayer)
                        {
                            child_layers.push_back(ChannelSet(viewlayer, layer.part, {}));
                        }

                        child_layers.back().channels.push_back(channel);
                    }
                    // append children layer
                    for (const auto& layer : child_layers) {
                        layers_by_delimiter.push_back(layer);
                    }

                    //layer.channels = std::vector<Channel>();
                    //layer.children = child_layers;
                }
                this->_channel_sets = layers_by_delimiter;
            }

            /// group layers by patterns
            {
                const std::vector<std::vector<std::string>> patterns = {
                    {"red", "green", "blue"},
                    {"R", "G", "B", "A"}, {"R", "G", "B"}, {"R", "G"},
                    {"A", "B", "G", "R"},{"B", "G", "R"}, {"G", "R"},
                    {"x", "y", "z"}, {"x", "y"},
                    {"u", "v", "w"}, {"u", "v"}
                };
                std::vector<ChannelSet> layers_by_patterns;
                for (const auto& layer : this->_channel_sets)
                {
                    std::vector<ChannelSet> child_layers;
                    std::vector<std::string> channel_names;
                    for (const auto& channel : layer.channels)
                    {
                        auto [_, channel_name] = split_channel_id(channel);
                        channel_names.push_back(channel_name);
                    }

                    // enumerate channel groups
                    for (const auto& [begin, end] : group_stringvector_by_patterns(channel_names, patterns))
                    {
                        auto channels_by_pattern = std::vector<std::string>(layer.channels.begin() + begin, layer.channels.begin() + end);
                        child_layers.push_back({ layer.name, layer.part, channels_by_pattern });
                    }

                    // append children layer
                    for (const auto& layer : child_layers) {
                        layers_by_patterns.push_back(layer);
                    }
                }
                this->_channel_sets = layers_by_patterns;
            }
        }

        std::tuple<int, int> range() const
        {
            return { _first_frame, _last_frame };
        }

        bool seek(int frame) override
        {
            if (_current_frame == frame) return true;
            if (frame<_first_frame || frame>_last_frame) return false;
            _current_frame = frame;
            if (_is_sequence) {
                _current_filepath = sequence_item(_sequence_path, _current_frame);
                _current_input = OIIO::ImageInput::open(_current_filepath.string());
                return true;
            }
            return true;
        }

        Info info() const override
        {
            auto spec = _current_input->spec();
            return {
                spec.x,spec.y,spec.width,spec.height,
                {"R", "G", "B", "A"},
                spec.format
            };
        }

        bool read(void* data, ChannelSet channel_set=ChannelSet()) override
        {
            return _current_input->read_image(0, 4, _current_input->spec().format, data);
        }

        std::vector<ChannelSet> channel_sets() {
            return _channel_sets;
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

        bool seek(int frame) override
        {
            if (cap.get(cv::CAP_PROP_POS_FRAMES) != frame)
            {
                cap.set(cv::CAP_PROP_POS_FRAMES, frame);
            }
            return true;
        }

        Info info() const override
        {
            return {
                0,0,
                (int)cap.get(cv::CAP_PROP_FRAME_WIDTH),
                (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT),
                {"B", "G", "R"},
                OIIO::TypeUInt8
            };
        }
        
        bool read(void* data, ChannelSet channel_set = ChannelSet()) override
        {
            auto spec = info();
            
            cv::Mat mat(spec.height, spec.width, CV_8UC3, data);
            bool success = cap.read(mat);
            //cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
            return success;
        }

        std::vector<ChannelSet> channel_sets()
        {
            return { ChannelSet("color", 0, { "R", "G", "B" }) };
        }
    };

    // Factory
    std::unique_ptr<MovieInput> MovieInput::open(const std::string& name)
    {
        auto ext = std::filesystem::path(name).extension();
        if (std::set<std::filesystem::path>({ ".mp4" }).contains(ext))
        {
            return std::make_unique<OpenCVMovieInput>(name);
        }
        else if (std::set<std::filesystem::path>({ ".exr", ".jpg", ".jpeg"}).contains(ext))
        {
            return std::make_unique<OIIOMovieInput>(name);
        }
    }
}