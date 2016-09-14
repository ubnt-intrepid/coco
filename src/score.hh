#ifndef __HEADER_ALGO__
#define __HEADER_ALGO__

#include <regex>
#include <string>
#include <utility>
#include <limits>

auto score_by_regex(std::string const& query)
{
  std::regex re(query);
  return [re = std::move(re)](std::string const& line)->std::size_t
  {
    return std::regex_search(line, re) ? 0 : std::numeric_limits<std::size_t>::max();
  };
}

#endif