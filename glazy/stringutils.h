#pragma once

#include <string>
#include <vector>
#include <tuple>


/**
  example:
  split_string("hello_world", "_")
  {"hello", "world"}
*/
std::vector<std::string> split_string(const std::string& text, const std::string& delimiter);

/*
  joint_string({"andris", "judit", "masa"}, ", ")
  "andris, judit, masa"
*/
std::string join_string(const std::vector<std::string>& segments, const std::string& delimiter, int range_start=0, int range_end=-1);


/*
  Split digits from the end of the string
  split_digits("hello_4556")
  {"hello", "4556"}
*/
std::tuple<std::string, std::string> split_digits(const std::string& stem);


/// Test if text starts with a specific string
/// return true if so
inline bool starts_with(const std::string& text, const std::string& begining) {
    if (text.length() >= begining.length()) {
        return (0 == text.compare(0, begining.length(), begining));
    }
    else {
        return false;
    }
}

/// Test if text ends with a specific string
/// return true if so
inline bool ends_with(const std::string& text, const std::string& ending) {
    if (text.length() >= ending.length()) {
        return (0 == text.compare(text.length() - ending.length(), ending.length(), ending));
    }
    else {
        return false;
    }
}

inline std::string trim(std::string& str, const char* ws = " \t\n\r\f\v")
{
    str.erase(str.find_last_not_of(ws) + 1);         //suffixing spaces
    str.erase(0, str.find_first_not_of(ws));       //prefixing spaces
    return str;
}