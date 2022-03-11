#pragma once
#include <vector>
#include <iostream>
#include <string>
#include <format>

std::vector<std::tuple<int, int>> group_stringvector_by_patterns(const std::vector<std::string>& statement, const std::vector<std::vector<std::string>>& patterns) {

    std::vector<std::tuple<int, int>> groups;

    // sort patterns
    auto sorted_patterns = patterns;
    sort(sorted_patterns.begin(), sorted_patterns.end(), [](const std::vector<std::string>& A, const std::vector<std::string>& B) {
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

class Layer {
private:
    std::string mName;
    std::vector<std::string> mChannels;

    std::string mPart_name;
    int mPart_idx;
    std::vector<std::string> full_channel_name;
public:
    Layer(std::string name, std::vector<std::string> channels, std::string part_name, int part_idx) : mName(name), mChannels(channels), mPart_name(part_name), mPart_idx(part_idx) {}

    std::string part_name() const {
        return mPart_name;
    }

    int part_idx() const {
        return mPart_idx;
    }

    /// the layer name
    std::string name() const {
        return mName;
    }

    /// full EXR channel name
    std::vector<std::string> channel_ids() const {
        std::vector<std::string> full_names;
        for (auto channel : mChannels) {
            full_names.push_back(mName.empty() ? channel : join_string({ mName, channel }, "."));
        }
        return full_names;
    }

    Layer sublayer(std::vector<std::string> subchannels, std::string sublayer_name = "", bool compose = true) const
    {
        // keep channels on this layer only
        auto channel_names = this->channels();
        std::erase_if(subchannels, [&](std::string c) {
            return std::find(channel_names.begin(), channel_names.end(), c) == channel_names.end();
        });

        //// compose part and layer name
        if (compose) {
            std::vector<std::string> names{ this->mName, sublayer_name };
            names.erase(std::remove_if(names.begin(), names.end(), [](const std::string& s) {return s.empty(); }), names.end());
            sublayer_name = join_string(names, ".");
        }

        return Layer(sublayer_name, subchannels, this->part_name(), this->part_idx());
    }

    static std::tuple<std::string, std::string> split_channel_id(const std::string& channel_id)
    {
        size_t found = channel_id.find_last_of(".");
        auto viewlayer = found != std::string::npos ? channel_id.substr(0, found) : "";
        auto channel = channel_id.substr(found + 1);
        return { viewlayer, channel };
    }

    std::vector<Layer> split_by_delimiter() const {
        std::vector<Layer> sublayers;

        for (std::string channel_id : mChannels) {
            auto [viewlayer, channel] = Layer::split_channel_id(channel_id);



            if (sublayers.empty())
            {
                sublayers.push_back(sublayer({}, viewlayer));
            }

            if (sublayers.back().mName == viewlayer) {

            }
            else
            {
                sublayers.push_back(sublayer({}, viewlayer));
            }

            sublayers.back().mChannels.push_back(channel);
        }

        return sublayers;
    }

    std::vector<Layer> split_by_patterns(const std::vector<std::vector<std::string>>& patterns) const {
        std::vector<Layer> sublayers;

        for (auto span : group_stringvector_by_patterns(mChannels, patterns)) {
            auto [begin, end] = span;
            //Layer sublayer{ this->mName,  };
            auto channels = std::vector<std::string>(mChannels.begin() + begin, mChannels.begin() + end);
            sublayers.push_back(sublayer(channels));
        }

        return sublayers;
    }

    std::vector<std::string> channels() const {
        return mChannels;
    }

    /// nice name
    std::string display_name() const {
        std::stringstream ss;
        
        auto part_idx = this->part_idx();
        auto part_name = this->part_name();
        auto layer_name = this->name();
        auto channels = this->channels();

        std::string name;

        if (part_name != layer_name) {
            if (!part_name.empty() && !layer_name.empty()) {
                ss << part_name << "/" << layer_name;
            }
            else {
                ss << part_name << layer_name;
            }
        }
        else {
            ss << layer_name;
        }

        ss <<  " (";
        for (auto c : this->mChannels) ss << c;
        ss << ")";
        return ss.str();
    }

    /// support stringtream
    friend auto operator<<(std::ostream& os, Layer const& lyr) -> std::ostream& {
        os << lyr.display_name();
        return os;
    }
};

namespace Test {
    int test_layer_class()
    {
        std::cout << "\nTEST: group_by_patterns" << "\n";
        std::vector<std::string>statement = { "red", "green", "blue", "A", "B", "G", "R", "Z", "u", "v", "w", "u", "v", "x", "y", "k", "B", "G", "R" };
        std::vector<std::vector<std::string>> patterns = { {"red", "green", "blue"},{"A", "B", "G", "R"},{"B", "G", "R"},{"x", "y"},{"u", "v"},{"u", "v", "w"} };

        auto matches = group_stringvector_by_patterns(statement, patterns);

        for (auto span : matches)
        {
            auto [begin, end] = span;
            std::cout << "(" << begin << ", " << end << ")";
            std::vector<std::string> matchin_items = std::vector<std::string>(statement.begin() + begin, statement.begin() + end);

            std::cout << " ";
            for (auto it = statement.begin() + begin; it != statement.begin() + end; ++it)
            {
                std::cout << *it << ", ";
            }
            std::cout << "\n";
        }

        {
            std::cout << "\nTEST: Layer::group_by_delimiters" << "\n";
            std::vector<std::string>cl = { "A",
                "B",
                "G",
                "R",
                "Z",
                "disparityL.x",
                "disparityL.y",
                "disparityR.x",
                "disparityR.y",
                "forward.left.u",
                "forward.left.v",
                "forward.right.u",
                "forward.right.v",
                "left.A",
                "left.B",
                "left.G",
                "left.R",
                "left.Z",
                "whitebarmask.left.mask",
                "whitebarmask.right.mask" };
            Layer lyr("", cl, "", 0);
            auto sublayers = lyr.split_by_delimiter();
            //std::cout << lyr << "\n";


            for (auto sublyr : sublayers) {
                std::cout << sublyr << "\n";
            }
        }

        {
            std::cout << "\nTEST: Layer::group_by_patterns" << "\n";
            auto lyr = Layer("color", { "red", "green", "blue", "A", "B", "G", "R", "Z", "u", "v", "w", "u", "v", "x", "y", "k", "B", "G", "R" }, "", 0);


            for (auto sublyr : lyr.split_by_patterns(patterns))
            {
                std::cout << sublyr << "\n";
            }
        }
        return EXIT_SUCCESS;
    }
}