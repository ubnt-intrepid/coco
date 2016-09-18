#ifndef __HEADER_ALGO__
#define __HEADER_ALGO__

#include <string>
#include <memory>

enum FilterMode {
  CaseSensitive = 0,
  SmartCase = 1,
  Regex = 2,
};

class Filter {
public:
  virtual ~Filter() = default;
  virtual double operator()(std::string const& line) const = 0;
};

std::unique_ptr<Filter> score_by(FilterMode mode, std::string const& query);

#endif