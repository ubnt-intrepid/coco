#include "coco.hh"

#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

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
  Updated,
  Skip,
};

enum class Coco::Keymap {
  FinishSelectition,
  CancelSelection,
  CursorIncrement,
  CursorDecrement,
  ToggleSelection,
  RotateFilter,
  PopQuery,
  PushQuery,
  Unknown,
};

constexpr size_t y_offset = 1;

void Config::parse_args(int argc, char const** argv)
{
  cmdline::parser parser;
  parser.set_program_name("coco");
  parser.add("help", 'h', "print this message");
  parser.add<std::string>("query", 'q', "initial value for query", false, "");
  parser.add<std::string>("prompt", 'p', "specify the prompt string", false, "QUERY> ");
  parser.add<std::size_t>("max-buffer", 'b', "maximum length of lines", false, 4096);
//  parser.add<double>("score-min", 's', "threshold of score", false, 0.01);
  parser.add<std::string>("filter", 'f', "type of filter", false, "SmartCase",
                          cmdline::oneof<std::string>("CaseSensitive", "SmartCase", "Regex"));
  parser.footer("filename...");
  parser.parse_check(argc, argv);

  query = parser.get<std::string>("query");
  prompt = parser.get<std::string>("prompt");
// score_min = parser.get<double>("score-min");
  max_buffer = parser.get<std::size_t>("max-buffer");

  std::stringstream ss{parser.get<std::string>("filter")};
  ss >> filter_mode;

  if (parser.rest().size() > 0) {
    file = parser.rest()[0];
  }
}

Choices::Choices(arc<std::vector<std::string>> lines, receiver<bool> rx, double score_min)
    : lines(lines), rx(std::move(rx)), score_min(score_min)
{
  choices.resize(lines.read().get().size());
  std::generate(choices.begin(), choices.end(), [n = 0]() mutable { return Choice(n++); });
  filtered_len = choices.size();
}

void Choices::apply_filter(FilterMode mode, std::string const& query)
{
  try {
    auto scorer = score_by(mode, query);
    scorer->scoring(choices, lines.read().get());
    filtered_len =
        std::find_if(choices.begin(), choices.end(), [=](auto& choice) { return choice.score <= score_min; }) -
        choices.begin();
  }
  catch (std::regex_error&) {
  }

  // mark hidden choices unselected.
  for (size_t i = filtered_len; i < choices.size(); ++i) {
    choices[i].selected = false;
  }
}

std::vector<std::string> Choices::get_selection(std::size_t idx)
{
  std::vector<std::string> candidates;
  for (std::size_t i = 0; i < filtered_len; ++i) {
    if (choices[i].selected)
      candidates.push_back(lines.read().get()[choices[i].index]);
  }

  if (candidates.empty() && filtered_len > 0) {
    return {lines.read().get()[choices[idx].index]};
  }
  else {
    return candidates;
  }
}

Coco::Coco(Config const& config, Choices choices) : config(config), choices(std::move(choices))
{
  query = config.query;
  filter_mode = config.filter_mode;

  update_filter_list();
}

std::vector<std::string> Coco::select_line()
{
  // initialize ncurses screen.
  Window term;
  render_screen(term);

  // event loop.
  while (true) {
    auto result = handle_key_event(term);

    if (result == Status::Selected) {
      return choices.get_selection(cursor + offset);
    }
    else if (result == Status::Escaped) {
      break;
    }
    else if (result == Status::Updated) {
      render_screen(term);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return {};
}

void Coco::render_screen(Window& term)
{
  term.erase();

  int width, height;
  std::tie(width, height) = term.get_size();

  for (size_t y = 0; y < std::min<size_t>(choices.size() - offset, height - 1); ++y) {
    if (choices.is_selected(y + offset)) {
      term.add_str(0, y + y_offset, ">");
    }
    term.add_str(2, y + 1, choices.line(y + offset));

    if (y == cursor) {
      term.change_attr(0, y + 1, -1, 0);
    }
  }

  std::string query_str = config.prompt + query;

  std::stringstream ss;
  ss << filter_mode << " [" << cursor + offset << "/" << choices.size() << "]";
  std::string mode_str = ss.str();

  term.add_str(width - 1 - mode_str.length(), 0, mode_str);
  term.add_str(0, 0, query_str);

  term.refresh();
}

auto Coco::apply_keymap(Event ev, std::string& ch) -> Keymap
{
  if (ev == Key::Enter) {
    return Keymap::FinishSelectition;
  }
  else if (ev == Key::Esc) {
    return Keymap::CancelSelection;
  }
  else if (ev == Key::Up) {
    return Keymap::CursorDecrement;
  }
  else if (ev == Key::Down) {
    return Keymap::CursorIncrement;
  }
  else if (ev == Key::Tab) {
    return Keymap::ToggleSelection;
  }
  else if (ev == Key::Backspace) {
    return Keymap::PopQuery;
  }
  else if (ev == Key::Char) {
    ch = ev.as_chars();
    return Keymap::PushQuery;
  }
  else if (ev == Key::Ctrl) {
    if (ev.get_mod() == 'r') {
      return Keymap::RotateFilter;
    }
  }
  return Keymap::Unknown;
}

auto Coco::handle_key_event(Window& term) -> Status
{
  std::string ch;
  auto keymap = apply_keymap(term.poll_event(), ch);

  switch (keymap) {
  case Keymap::FinishSelectition:
    return Status::Selected;

  case Keymap::CancelSelection:
    return Status::Escaped;

  case Keymap::CursorDecrement: {
    if (cursor == 0) {
      offset = std::max(0, (int)offset - 1);
    }
    else {
      cursor--;
    }
    return Status::Updated;
  }
  case Keymap::CursorIncrement: {
    int height;
    std::tie(std::ignore, height) = term.get_size();

    if (cursor == static_cast<size_t>(height - 1 - y_offset)) {
      offset = std::min<size_t>(offset + 1, std::max<int>(0, choices.size() - height + y_offset));
    }
    else {
      cursor = std::min<size_t>(cursor + 1, std::min<size_t>(choices.size() - offset, height - y_offset) - 1);
    }
    return Status::Updated;
  }
  case Keymap::ToggleSelection: {
    choices.toggle_selection(cursor + offset);
    return Status::Updated;
  }
  case Keymap::PopQuery: {
    if (!query.empty()) {
      pop_back_utf8(query);
      update_filter_list();
    }
    return Status::Updated;
  }
  case Keymap::PushQuery: {
    query += ch;
    update_filter_list();
    return Status::Updated;
  }

  case Keymap::RotateFilter: {
    filter_mode = static_cast<FilterMode>((static_cast<int>(filter_mode) + 1) % 3);
    update_filter_list();
    return Status::Updated;
  }
  default:
    return Status::Skip;
  }
}

void Coco::update_filter_list()
{
  choices.apply_filter(filter_mode, query);

  // reset cursor
  cursor = 0;
  offset = 0;
}
