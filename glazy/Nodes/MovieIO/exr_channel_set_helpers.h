#pragma once
#include <tuple>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <regex>

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

