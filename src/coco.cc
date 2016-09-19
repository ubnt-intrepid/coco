#include "coco.hh"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
#include <tuple>

#include <nanojson.hpp>
#include <cmdline.h>
#include "filter.hh"
#include "ncurses.hh"
#include "utf8.hh"

using curses::Window;
using curses::Event;
using curses::Key;

enum class Coco::Status {
  Selected,
  Escaped,
  Continue,
};

constexpr size_t y_offset = 1;

void read_lines(std::vector<std::string>& lines, std::istream& is, std::size_t max_len)
{
  static std::regex ansi(R"(\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K])");
  for (std::string line; std::getline(is, line);) {
    if (lines.size() == max_len)
      return;
    lines.push_back(std::regex_replace(line, ansi, ""));
  }
}

Candidates Config::parse_args(int argc, char const** argv)
{
  cmdline::parser parser;
  parser.set_program_name("coco");
  parser.add<std::string>("query", 0, "initial value for query", false, "");
  parser.add<std::string>("prompt", 0, "specify the prompt string", false, "QUERY> ");
  parser.add<std::size_t>("max-buffer", 'b', "maximum length of lines", false, 4096);
  parser.add<double>("score-min", 's', "threshold of score", false, 0.01);
  parser.add<std::string>("filter", 'f', "type of filter", false, "SmartCase",
                          cmdline::oneof<std::string>("CaseSensitive", "SmartCase", "Regex"));
  parser.footer("filename...");
  parser.parse_check(argc, argv);

  query = parser.get<std::string>("query");
  prompt = parser.get<std::string>("prompt");
  score_min = parser.get<double>("score-min");
  max_buffer = parser.get<std::size_t>("max-buffer");
  filter = parser.get<std::string>("filter");

  std::vector<std::string> lines;
  lines.reserve(max_buffer);
  if (parser.rest().size() > 0) {
    for (auto&& path : parser.rest()) {
      std::ifstream ifs{path};
      read_lines(lines, ifs, max_buffer);
    }
  }
  else {
    read_lines(lines, std::cin, max_buffer);
  }
  return Candidates{std::move(lines)};
}

Coco::Coco(Config const& config, Candidates candidates) : config(config), candidates(candidates)
{
  query = config.query;

  std::stringstream ss{config.filter};
  ss >> filter_mode;

  choices.resize(candidates.lines.size());
  std::generate(choices.begin(), choices.end(), [n = 0]() mutable { return Choice(n++); });
  update_filter_list();
}

std::vector<std::string> Coco::select_line()
{
  // initialize ncurses screen.
  Window term;
  render_screen(term);

  while (true) {
    auto result = handle_key_event(term);

    if (result == Status::Selected) {
      std::vector<std::string> lines;
      for (std::size_t i = 0; i < filtered_len; ++i) {
        if (choices[i].selected)
          lines.push_back(candidates.lines[choices[i].index]);
      }
      if (lines.empty()) {
        return {candidates.lines[cursor + offset]};
      }
      else {
        return lines;
      }
    }
    else if (result == Status::Escaped) {
      break;
    }

    render_screen(term);
  }

  return {};
}

void Coco::render_screen(Window& term)
{
  term.erase();

  int width, height;
  std::tie(width, height) = term.get_size();

  for (size_t y = 0; y < std::min<size_t>(filtered_len - offset, height - 1); ++y) {
    if (choices[y + offset].selected) {
      term.add_str(0, y + y_offset, ">");
    }
    term.add_str(2, y + 1, candidates.lines[choices[y + offset].index]);

    if (y == cursor) {
      term.change_attr(0, y + 1, -1, 0);
    }
  }

  std::string query_str = config.prompt + query;

  std::stringstream ss;
  ss << filter_mode << " [" << cursor + offset << "/" << filtered_len << "]";
  std::string mode_str = ss.str();

  term.add_str(width - 1 - mode_str.length(), 0, mode_str);
  term.add_str(0, 0, query_str);

  term.refresh();
}

auto Coco::handle_key_event(Window& term) -> Status
{
  auto ev = term.poll_event();

  if (ev == Key::Enter) {
    return Status::Selected;
  }
  else if (ev == Key::Esc) {
    return Status::Escaped;
  }
  else if (ev == Key::Up) {
    if (cursor == 0) {
      offset = std::max(0, (int)offset - 1);
    }
    else {
      cursor--;
    }
    return Status::Continue;
  }
  else if (ev == Key::Down) {
    int height;
    std::tie(std::ignore, height) = term.get_size();

    if (cursor == static_cast<size_t>(height - 1 - y_offset)) {
      offset = std::min<size_t>(offset + 1, std::max<int>(0, filtered_len - height + y_offset));
    }
    else {
      cursor = std::min<size_t>(cursor + 1, std::min<size_t>(filtered_len - offset, height - y_offset) - 1);
    }
    return Status::Continue;
  }
  else if (ev == Key::Tab) {
    choices[cursor + offset].selected ^= true;
    return Status::Continue;
  }
  else if (ev == Key::Backspace) {
    if (!query.empty()) {
      pop_back_utf8(query);
      update_filter_list();
    }
    return Status::Continue;
  }
  else if (ev == Key::Ctrl) {
    if (ev.get_mod() == 'r') {
      filter_mode = static_cast<FilterMode>((static_cast<int>(filter_mode) + 1) % 3);
      update_filter_list();
    }
    return Status::Continue;
  }
  else if (ev == Key::Char) {
    query += ev.as_chars();
    update_filter_list();
    return Status::Continue;
  }
  else {
    return Status::Continue;
  }
}

void Coco::update_filter_list()
{
  try {
    auto scorer = score_by(filter_mode, query);
    scorer->scoring(choices, candidates.lines);
    filtered_len = std::find_if(choices.begin(), choices.end(),
                                [this](auto& choice) { return choice.score <= config.score_min; }) -
                   choices.begin();
  }
  catch (std::regex_error&) {
  }

  // mark hidden choices unselected.
  for (size_t i = filtered_len; i < choices.size(); ++i) {
    choices[i].selected = false;
  }

  // reset cursor
  cursor = 0;
  offset = 0;
}
