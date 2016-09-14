#ifndef __HEADER_COCO__
#define __HEADER_COCO__

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

struct Config {
  std::vector<std::string> lines;
  std::string prompt;
  std::string query;
  double score_min;
  std::size_t max_buffer;
  bool multi_select;
  std::string filter;

public:
  Config() = default;
  Config(int argc, char const** argv) { read_from(argc, argv); }

  void read_from(int argc, char const** argv);
  void read_lines(std::istream& is, std::size_t max_len);
};

struct Choice {
  std::size_t index;
  double score = 0;
  bool selected = false;

public:
  Choice() = default;
  Choice(std::size_t index) : index(index) {}

  bool operator>(Choice const& rhs) const { return score > rhs.score; }
};

enum FilterMode {
  CaseSensitive = 0,
  SmartCase = 1,
  Regex = 2,
};

// represents a instance of Coco client.
class Ncurses;
class Event;
class Coco {
  enum class Status;

  Config config;
  std::string query;

  std::vector<Choice> choices;
  std::size_t filtered_len = 0;

  std::size_t cursor = 0;
  std::size_t offset = 0;

  FilterMode filter_mode;

public:
  Coco(Config const& config);
  std::vector<std::string> select_line();

private:
  void render_screen(Ncurses& term);
  Status handle_key_event(Ncurses& term, Event const& ev);
  void update_filter_list();

  template <typename Scorer> void scoring(std::vector<std::string> const& lines, Scorer score);
};

#endif
