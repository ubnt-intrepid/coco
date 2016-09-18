#ifndef __HEADER_ALGO__
#define __HEADER_ALGO__

#include <vector>
#include <string>
#include <memory>
#include "choice.hh"

enum FilterMode {
  CaseSensitive = 0,
  SmartCase = 1,
  Regex = 2,
};

class Filter {
  std::string query;

public:
  Filter(std::string const& query) : query{query} {}

  virtual ~Filter() = default;
  virtual double operator()(std::string const& line) const = 0;
  void scoring(std::vector<Choice>& choices, std::vector<std::string> const& lines);
};

std::unique_ptr<Filter> score_by(FilterMode mode, std::string const& query);

#endif