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


/// Test if text ends with a specific string
/// return true if so
inline bool ends_with(std::string text, std::string ending) {
    if (text.length() >= ending.length()) {
        return (0 == text.compare(text.length() - ending.length(), ending.length(), ending));
    }
    else {
        return false;
    }
}

// trim from end of string (right)
inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from both ends of string (right then left)
inline std::string& trim(std::string& s, const char* t = " \t\n\r\f\v")
{
    return ltrim(rtrim(s, t), t);
}