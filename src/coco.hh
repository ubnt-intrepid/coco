#ifndef __HEADER_COCO__
#define __HEADER_COCO__

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "filter.hh"
#include "choice.hh"
#include "arc.hh"
#include "channel.hh"

namespace curses {
class Window;
class Event;
}

struct Config {
  std::string prompt;
  std::string query;
  double score_min;
  std::size_t max_buffer;
  FilterMode filter_mode;
  std::string file;

public:
  Config() = default;
  void parse_args(int argc, char const** argv);
};

class Choices {
  arc<std::vector<std::string>> lines;
  receiver<bool> rx;

  std::vector<Choice> choices;
  std::size_t filtered_len = 0;
  double score_min = 0.01;

public:
  Choices() = default;
  Choices(Choices&&) noexcept = default;
  Choices(arc<std::vector<std::string>> lines, receiver<bool> rx, double score_min);

  std::vector<std::string> get_selection(std::size_t index);
  void apply_filter(FilterMode mode, std::string const& query);
  bool is_selected(size_t index) { return choices[index].selected; }
  void toggle_selection(std::size_t index) { choices[index].selected ^= true; }
  std::size_t size() const noexcept { return filtered_len; }
  std::string line(std::size_t index) { return lines.read().get()[choices[index].index]; }
};

// represents a instance of Coco client.
class Coco {
  enum class Status;
  enum class Keymap;

  Config config;
  Choices choices;

  std::string query;
  FilterMode filter_mode;
  std::size_t cursor = 0;
  std::size_t offset = 0;

public:
  Coco(Config const& config, Choices choices);
  std::vector<std::string> select_line();

private:
  void render_screen(curses::Window& term);
  Status handle_key_event(curses::Window& term);
  void update_filter_list();
  Keymap apply_keymap(curses::Event ev, std::string& ch);
};

#endif
