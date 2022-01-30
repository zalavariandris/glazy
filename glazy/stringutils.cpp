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

std::string join_string(const std::vector<std::string>& segments, const std::string& delimiter, int range_start, int range_end) {
    if (range_end < 0) range_end = segments.size();
    assert(("invalid range", range_start < range_end));
    std::string result;

    // append all +delimiter but last item
    for (auto i = range_start; i < range_end-1; i++) {
        result += segments[i];
        result += delimiter;
    }
    // append last item
    result += segments[range_end-1];
    return result;
}

std::tuple<std::string, std::string> split_digits(const std::string& stem) {
    if (stem.empty()) return { "","" };

    int digits_count = 0;
    while (stem.size()>digits_count && std::isdigit(stem[stem.size() - 1 - digits_count])) {
        digits_count++;
    }

    auto text = stem.substr(0, stem.size() - digits_count);
    auto digits = stem.substr(stem.size() - digits_count, -1);

    return { text, digits };
}