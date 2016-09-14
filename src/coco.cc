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
#include "score.hh"
#include "ncurses.hh"
#include "utf8.hh"

constexpr size_t y_offset = 1;

void Config::read_from(int argc, char const** argv)
{
  cmdline::parser parser;
  parser.set_program_name("coco");
  parser.add<std::string>("query", 0, "initial value for query", false, "");
  parser.add<std::string>("prompt", 0, "specify the prompt string", false, "QUERY> ");
  parser.add<std::size_t>("max-buffer", 'b', "maximum length of lines", false, 4096);
  parser.add<double>("score-min", 's', "threshold of score", false, 0.01);
  parser.add("multi-select", 'm', "enable multiple selection of lines");
  parser.footer("filename...");
  parser.parse_check(argc, argv);

  query = parser.get<std::string>("query");
  prompt = parser.get<std::string>("prompt");
  score_min = parser.get<double>("score-min");
  max_buffer = parser.get<std::size_t>("max-buffer");
  multi_select = parser.exist("multi-select");

  lines.resize(0);
  lines.reserve(max_buffer);
  if (parser.rest().size() > 0) {
    for (auto&& path : parser.rest()) {
      std::ifstream ifs{path};
      read_lines(ifs, max_buffer);
    }
  }
  else {
    read_lines(std::cin, max_buffer);
  }
}

void Config::read_lines(std::istream& is, std::size_t max_len)
{
  static std::regex ansi(R"(\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K])");
  for (std::string line; std::getline(is, line);) {
    if (lines.size() == max_len)
      return;
    lines.push_back(std::regex_replace(line, ansi, ""));
  }
}

Coco::Coco(Config const& config) : config(config)
{
  query = config.query;

  choices.resize(config.lines.size());
  std::generate(choices.begin(), choices.end(), [n = 0]() mutable { return Choice(n++); });
  update_filter_list();
}

std::vector<std::string> Coco::select_line()
{
  // initialize ncurses screen.
  Ncurses term;
  render_screen(term);

  while (true) {
    Event ev = term.poll_event();
    auto result = handle_key_event(term, ev);

    if (result == Status::Selected) {
      std::vector<std::string> lines;
      for (std::size_t i = 0; i < filtered_len; ++i) {
        if (choices[i].selected)
          lines.push_back(config.lines[choices[i].index]);
      }
      return lines;
    }
    else if (result == Status::Escaped) {
      break;
    }

    render_screen(term);
  }

  return {};
}

void Coco::render_screen(Ncurses& term)
{
  std::string query_str = config.prompt + query;

  term.erase();

  int height;
  std::tie(std::ignore, height) = term.get_size();

  for (size_t y = 0; y < std::min<size_t>(filtered_len - offset, height - 1); ++y) {
    if (choices[y + offset].selected) {
      term.add_str(0, y + y_offset, ">");
    }
    term.add_str(2, y + 1, config.lines[choices[y + offset].index]);

    if (y == cursor) {
      term.change_attr(0, y + 1, -1, 0);
    }
  }

  term.add_str(0, 0, query_str);

  term.refresh();
}

auto Coco::handle_key_event(Ncurses& term, Event const& ev) -> Status
{
  if (ev == Key::Enter) {
    return Status::Selected;
  }
  else if (ev == Key::Esc) {
    return Status::Escaped;
  }
  else if (ev == Key::Up) {
    if (!config.multi_select) {
      choices[cursor + offset].selected = false;
    }
    if (cursor == 0) {
      offset = std::max(0, (int)offset - 1);
    }
    else {
      cursor--;
    }
    if (!config.multi_select) {
      choices[cursor + offset].selected = true;
    }
    return Status::Continue;
  }
  else if (ev == Key::Down) {
    int height;
    std::tie(std::ignore, height) = term.get_size();

    if (!config.multi_select) {
      choices[cursor + offset].selected = false;
    }

    if (cursor == static_cast<size_t>(height - 1 - y_offset)) {
      offset = std::min<size_t>(offset + 1, std::max<int>(0, filtered_len - height + y_offset));
    }
    else {
      cursor = std::min<size_t>(cursor + 1, std::min<size_t>(filtered_len - offset, height - y_offset) - 1);
    }

    if (!config.multi_select) {
      choices[cursor + offset].selected = true;
    }
    return Status::Continue;
  }
  else if (ev == Key::Tab) {
    if (config.multi_select) {
      choices[cursor + offset].selected ^= true;
    }
    return Status::Continue;
  }
  else if (ev == Key::Backspace) {
    if (!query.empty()) {
      pop_back_utf8(query);
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
    scoring(config.lines, score_by_regex(query));

    filtered_len = std::find_if(choices.begin(), choices.end(),
                                [this](auto& choice) { return choice.score <= config.score_min; }) -
                   choices.begin();
  }
  catch (std::regex_error&) {
  }

  cursor = 0;
  offset = 0;
  if (!config.multi_select) {
    for (auto& choice : choices) {
      choice.selected = false;
    }
    choices[0].selected = true;
  }
}

template <typename Scorer> void Coco::scoring(std::vector<std::string> const& lines, Scorer score)
{
  for (auto& choice : choices) {
    choice.score = query.empty() ? 1.0 : score(lines[choice.index]);
  }
  std::stable_sort(choices.begin(), choices.end(), std::greater<Choice>{});
}
