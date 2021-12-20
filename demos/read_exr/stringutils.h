#pragma once

#include <string>
#include <tuple>
#include <vector>

/**
  example:
  split_string("hello_world", "_")
  {"hello", "world"}
*/
inline std::vector<std::string> split_string(std::string text, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    while ((pos = text.find(delimiter)) != std::string::npos) {
        auto token = text.substr(0, pos);
        tokens.push_back(token);
        text.erase(0, pos + delimiter.length());
    }
    tokens.push_back(text);
    return tokens;
};

/*
  joint_string({"andris", "judit", "masa"}, ", ")
  "andris, judit, masa"
*/
inline std::string join_string(const std::vector<std::string>& tokens, const std::string& delimiter) {
    std::string text;
    // chan all but last items
    for (auto i = 0; i < tokens.size() - 1; i++) {
        text += tokens[i];
        text += delimiter;
    }
    // append last item
    text += tokens.back();
    return text;
}


/*
  split digits from the end of the string
  split_digits("hello_4556")
  {"hello", "4556"}
*/
inline std::tuple<std::string, std::string> split_digits(const std::string& stem) {
    assert(!stem.empty());

    int digits_count = 0;
    while (std::isdigit(stem[stem.size() - 1 - digits_count])) {
        digits_count++;
    }

    auto text = stem.substr(0, stem.size() - digits_count);
    auto digits = stem.substr(stem.size() - digits_count, -1);

    return { text, digits };
}