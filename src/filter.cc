#include "filter.hh"

#include <locale>
#include <algorithm>
#include <regex>
#include <utility>
#include <limits>
#include <sstream>

void Filter::scoring(std::vector<Choice>& choices, std::vector<std::string> const& lines)
{
  for (auto& choice : choices) {
    choice.score = query.empty() ? 1.0 : (*this)(lines[choice.index]);
  }
  std::stable_sort(choices.begin(), choices.end(), std::greater<Choice>{});
}

class CaseSensitiveFilter : public Filter {
  std::vector<std::string> words;

public:
  CaseSensitiveFilter(std::string const& query) : Filter{query}
  {
    std::istringstream iss(query);
    for (std::string word; std::getline(iss, word, ' ');) {
      if (word.find(" ") == std::string::npos) {
        words.push_back(word);
      }
    }
  }

  double operator()(std::string const& line) const override
  {
    return std::all_of(words.begin(), words.end(), [&](auto& word) { return line.find(word) != std::string::npos; })
               ? 1
               : 0;
  }
};

class SmartCaseFilter : public Filter {
  std::vector<std::string> words;
  std::locale l;

public:
  SmartCaseFilter(std::string const& query) : Filter{query}, l{std::locale::classic()}
  {
    std::istringstream iss(query);
    for (std::string word; std::getline(iss, word, ' ');) {
      if (word.find(" ") == std::string::npos) {
        words.push_back(word);
      }
    }
  }

  double operator()(std::string const& line) const override
  {
    return std::all_of(words.begin(), words.end(),
                       [&](auto& word) {
                         return std::search(line.begin(), line.end(), word.begin(), word.end(), [&](auto c1, auto c2) {
                                  return std::toupper(c1, l) == std::toupper(c2, l);
                                }) != line.end();
                       })
               ? 1
               : 0;
  }
};

class RegexFilter : public Filter {
  std::regex re;

public:
  RegexFilter(std::string const& query) : Filter{query}, re{query} {}

  double operator()(std::string const& line) const override { return std::regex_search(line, re) ? 1.0 : 0.0; }
};

std::unique_ptr<Filter> score_by(FilterMode mode, std::string const& query)
{
  switch (mode) {
  case FilterMode::CaseSensitive:
    return std::make_unique<CaseSensitiveFilter>(query);
  case FilterMode::SmartCase:
    return std::make_unique<SmartCaseFilter>(query);
  case FilterMode::Regex:
    return std::make_unique<RegexFilter>(query);
  default:
    throw std::runtime_error(std::string(__FUNCTION__) + ": invalid mode");
  }
}
