#pragma once

#include <string>
#include <vector>
#include <tuple>


/**
  example:
  split_string("hello_world", "_")
  {"hello", "world"}
*/
std::vector<std::string> split_string(std::string text, const std::string& delimiter);

/*
  joint_string({"andris", "judit", "masa"}, ", ")
  "andris, judit, masa"
*/
std::string join_string(const std::vector<std::string>& tokens, const std::string& delimiter);


/*
  split digits from the end of the string
  split_digits("hello_4556")
  {"hello", "4556"}
*/
std::tuple<std::string, std::string> split_digits(const std::string& stem);