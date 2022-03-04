#pragma once
#include <vector>
#include <iostream>
#include <string>

std::vector<std::tuple<int, int>> group_stringvector_by_patterns(const std::vector<std::string>& statement, const std::vector<std::vector<std::string>>& patterns) {

    std::vector<std::tuple<int, int>> groups;

    // sort patterns
    auto sorted_patterns = patterns;
    sort(sorted_patterns.begin(), sorted_patterns.end(), [](const std::vector<std::string>& A, const std::vector<std::string>& B) {
        return A.size() > B.size();
        });

    // collect pattenr matches
    int i = 0;
    while (i < statement.size())
    {
        std::optional<std::tuple<int, int>> match;

        // check all patterns at current index
        // if match is found, move iterator to the end of match
        for (std::vector<std::string> pattern : sorted_patterns) {
            auto begin = i;
            auto end = i + pattern.size();
            if (pattern == std::vector<std::string>(statement.begin() + begin, std::min(statement.begin() + end, statement.end())))
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
public:
    std::string name;
    std::vector<std::string> channels;

    Layer(std::string name) : name(name) {}

    friend auto operator<<(std::ostream& os, Layer const& m) -> std::ostream& {
        os << "Layer('" << m.name << "': " << join_string(m.channels, ", ") << ")";
        return os;
    }

    static std::tuple<std::string, std::string> split_channel_id(const std::string& channel_id)
    {
        size_t found = channel_id.find_last_of(".");
        auto viewlayer = found != std::string::npos ? channel_id.substr(0, found) : "";
        auto channel = channel_id.substr(found + 1);
        return { viewlayer, channel };
    }

    std::vector<Layer> group_by_delimiter() const {
        std::vector<Layer> sublayers;

        for (std::string channel_id : channels) {
            auto [viewlayer, channel] = Layer::split_channel_id(channel_id);

            if (sublayers.empty()) {
                sublayers.push_back(Layer(viewlayer));
            }

            if (sublayers.back().name == viewlayer) {

            }
            else
            {
                sublayers.push_back(Layer(viewlayer));
            }

            sublayers.back().channels.push_back(channel);
        }

        return sublayers;
    }

    std::vector<Layer> group_by_patterns(const std::vector<std::vector<std::string>>& patterns) const {
        std::vector<Layer> sublayers;

        for (auto span : group_stringvector_by_patterns(channels, patterns)) {
            auto [begin, end] = span;
            Layer sublayer{ this->name };
            sublayer.channels = std::vector<std::string>(channels.begin() + begin, channels.begin() + end);
            sublayers.push_back(sublayer);
        }

        return sublayers;
    }

    std::vector<std::string> channel_ids() {
        std::vector<std::string> full_names;
        for (auto channel : channels) {
            full_names.push_back(name.empty() ? channel : join_string({ name,channel }, "."));
        }
        return full_names;
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
            Layer lyr("");
            lyr.channels = cl;
            auto sublayers = lyr.group_by_delimiter();
            //std::cout << lyr << "\n";


            for (auto sublyr : sublayers) {
                std::cout << sublyr << "\n";
            }
        }

        {
            std::cout << "\nTEST: Layer::group_by_patterns" << "\n";
            auto lyr = Layer("color");
            lyr.channels = { "red", "green", "blue", "A", "B", "G", "R", "Z", "u", "v", "w", "u", "v", "x", "y", "k", "B", "G", "R" };


            for (auto sublyr : lyr.group_by_patterns(patterns))
            {
                std::cout << sublyr << "\n";
            }
        }
        return EXIT_SUCCESS;
    }
}