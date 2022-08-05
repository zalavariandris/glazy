#include "OIIOMovieInput.h"
#include "../../pathutils.h" // sequence
#include "exr_channel_set_helpers.h"

namespace MovieIO
{
    OIIOMovieInput::OIIOMovieInput(std::string sequence_path)
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
        else if (std::filesystem::exists(sequence_path))
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

    void OIIOMovieInput::update_channel_sets() {
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
                for (int channel_idx = 0; channel_idx < spec.nchannels; channel_idx++)
                {
                    std::string channel_name = spec.channel_name(channel_idx);
                    channels.push_back(channel_name);
                    channel_idx_by_name[channel_name] = channel_idx;
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

    std::tuple<int, int> OIIOMovieInput::range() const
    {
        return { _first_frame, _last_frame };
    }

    bool OIIOMovieInput::seek(int frame)
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

    Info OIIOMovieInput::info() const
    {
        auto spec = _current_input->spec();
        return {
            spec.x,spec.y,spec.width,spec.height,
            {"R", "G", "B", "A"},
            spec.format
        };
    }

    bool OIIOMovieInput::read(void* data, ChannelSet channel_set)
    {
        if (_current_input->current_subimage() != channel_set.part) {
            _current_input->seek_subimage(channel_set.part, 0);
        }

        // collect channel indices
        std::vector<int> channel_indices;
        for (std::string channel_name : channel_set.channels) {
            int channel_idx = channel_idx_by_name.at(channel_name);
            channel_indices.push_back(channel_idx);
        }

        // find chbegin and chend
        const auto [chbegin_it, chend_it] = std::minmax_element(channel_indices.begin(), channel_indices.end());
        int chbegin = *chbegin_it;
        int chend = *chend_it;
        if (chbegin + 4 > chend) throw "maximum 4 channels exceeded";
        
        //_current_input->read_image(subimage, miplevel, chbegin, chend, format)
        return _current_input->read_image(0, 4, _current_input->spec().format, data);
    }

    std::vector<ChannelSet> OIIOMovieInput::channel_sets() const {
        return _channel_sets;
    }
}