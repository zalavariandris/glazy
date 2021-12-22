#include "stringutils.h"
#include <cassert>

std::vector<std::string> split_string(const std::string& text, const std::string& delimiter) {
    std::vector<std::string> tokens;
    std::string _text(text);
    size_t pos = 0;
    while ((pos = _text.find(delimiter)) != std::string::npos) {
        auto token = _text.substr(0, pos);
        tokens.push_back(token);
        _text.erase(0, pos + delimiter.length());
    }
    tokens.push_back(_text);
    return tokens;
};

std::string join_string(const std::vector<std::string>& tokens, const std::string& delimiter) {
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

std::tuple<std::string, std::string> split_digits(const std::string& stem) {
    if (stem.empty()) return { "","" };

    int digits_count = 0;
    while (std::isdigit(stem[stem.size() - 1 - digits_count])) {
        digits_count++;
    }

    auto text = stem.substr(0, stem.size() - digits_count);
    auto digits = stem.substr(stem.size() - digits_count, -1);

    return { text, digits };
}