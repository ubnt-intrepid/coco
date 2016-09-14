#ifndef __HEADER_ALGO__
#define __HEADER_ALGO__

#include <locale>
#include <algorithm>
#include <regex>
#include <string>
#include <utility>
#include <limits>
#include <sstream>

auto score_case_sensitive(std::string const& query)
{
  std::vector<std::string> words;
  std::istringstream iss(query);
  for (std::string word; std::getline(iss, word, ' ');) {
    if (word.find(" ") == std::string::npos) {
      words.push_back(word);
    }
  }
  return [words = std::move(words)](std::string const& line)->double
  {
    return std::all_of(words.begin(), words.end(), [&](auto& word) { return line.find(word) != std::string::npos; })
               ? 1
               : 0;
  };
}

auto score_smart_case(std::string const& query)
{
  std::vector<std::string> words;
  std::istringstream iss(query);
  for (std::string word; std::getline(iss, word, ' ');) {
    if (word.find(" ") == std::string::npos) {
      words.push_back(word);
    }
  }
  return [ words = std::move(words), l = std::locale::classic() ](std::string const& line)->double
  {
    return std::all_of(words.begin(), words.end(),
                       [&](auto& word) {
                         return std::search(line.begin(), line.end(), word.begin(), word.end(), [&](auto c1, auto c2) {
                                  return std::toupper(c1, l) == std::toupper(c2, l);
                                }) != line.end();
                       })
               ? 1
               : 0;
  };
}

auto score_by_regex(std::string const& query)
{
  std::regex re(query);
  return [re = std::move(re)](std::string const& line)->double { return std::regex_search(line, re) ? 1.0 : 0.0; };
}

#endif