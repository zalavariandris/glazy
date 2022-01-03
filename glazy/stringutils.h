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
  split digits from the end of the string
  split_digits("hello_4556")
  {"hello", "4556"}
*/
std::tuple<std::string, std::string> split_digits(const std::string& stem);