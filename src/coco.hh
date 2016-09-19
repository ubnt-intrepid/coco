#ifndef __HEADER_COCO__
#define __HEADER_COCO__

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "filter.hh"
#include "choice.hh"

namespace curses {
class Window;
class Event;
}

struct Candidates {
  std::vector<std::string> lines;
};

struct Config {
  std::string prompt;
  std::string query;
  double score_min;
  std::size_t max_buffer;
  std::string filter;

public:
  Config() = default;
  Candidates parse_args(int argc, char const** argv);
};

// represents a instance of Coco client.
class Coco {
  enum class Status;

  Config config;
  Candidates candidates;
  std::string query;

  std::vector<Choice> choices;
  std::size_t filtered_len = 0;

  std::size_t cursor = 0;
  std::size_t offset = 0;

  FilterMode filter_mode;

public:
  Coco(Config const& config, Candidates candidates);
  std::vector<std::string> select_line();

private:
  void render_screen(curses::Window& term);
  Status handle_key_event(curses::Window& term);
  void update_filter_list();
};

#endif
