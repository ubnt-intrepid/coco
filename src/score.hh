#ifndef __HEADER_ALGO__
#define __HEADER_ALGO__

#include <regex>
#include <string>
#include <utility>
#include <limits>

auto score_by_regex(std::string const& query)
{
  std::regex re(query);
  return [re = std::move(re)](std::string const& line)->double { return std::regex_search(line, re) ? 1.0 : 0.0; };
}

#endif