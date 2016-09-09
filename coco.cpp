#include <termbox.hpp>
#include <nanojson.hpp>
#include <cmdline.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

enum class State {
  Continue,
  Escaped,
  Selected,
};

class CocoClient {
  TermBox termbox;

  // states
  std::vector<std::string> inputs, filtered, rendered;
  std::string query;
  long selected = 0;
  size_t offset = 0;

  // configurations
  std::string const query_header = "QUERY> ";
  static constexpr unsigned y_offset = 1;

public:
  CocoClient(std::vector<std::string> inputs) : inputs(inputs), filtered(inputs), rendered(inputs) {}

  std::tuple<bool, std::string> run()
  {
    render_items();

    while (true) {
      Event ev = termbox.poll_event();

      State state;
      std::string selected_str;
      std::tie(state, selected_str) = handle_event(ev);
      if (state == State::Selected) {
        return std::make_tuple(true, selected_str);
      }
      else if (state == State::Escaped) {
        break;
      }

      render_items();
    }

    return std::make_tuple(false, std::string{});
  }

private:
  std::tuple<State, std::string> handle_event(Event const& ev)
  {
    if (ev.is_key(TB_KEY_ENTER)) {
      return std::make_tuple(filtered.empty() ? State::Escaped : State::Selected, selected_line());
    }
    else if (ev.is_key(TB_KEY_ESC)) {
      return std::make_tuple(State::Escaped, std::string{});
    }
    else if (ev.is_key(TB_KEY_ARROW_UP)) {
      move_up();
    }
    else if (ev.is_key(TB_KEY_ARROW_DOWN)) {
      move_down();
    }
    else if (ev.is_key(TB_KEY_BACKSPACE) || ev.is_key(TB_KEY_BACKSPACE2)) {
      pop_query();
    }
    else if (ev.is_char()) {
      push_query(ev.as_char());
    }

    return std::make_tuple(State::Continue, std::string{});
  }

  std::string selected_line() const
  {
    return rendered.size() <= offset + selected ? std::string{} : rendered[offset + selected];
  }

  void push_query(uint32_t c)
  {
    query.push_back(c);
    apply_filter();
  }

  void pop_query()
  {
    if (!query.empty()) {
      query.pop_back();
      apply_filter();
    }
  }

  void move_up()
  {
    if (selected > -1) {
      selected -= 1;
    }

    if (selected == -1) {
      selected += 1;
      if (offset > 0) {
        offset -= 1;
        rendered.assign(filtered.begin() + offset, filtered.end());
      }
    }
  }

  void move_down()
  {
    if (((rendered.size() < termbox.height() - 1) && (selected < rendered.size() - 1)) ||
        ((rendered.size() > termbox.height() - 1) && (selected < termbox.height() - 1))) {
      selected += 1;
    }

    if (selected == termbox.height() - 1) {
      selected -= 1;
      if (offset < filtered.size() - 1) {
        offset += 1;
        rendered.assign(filtered.begin() + offset, filtered.end());
      }
    }
  }

  void apply_filter()
  {
    filtered = filter();
    rendered = filtered;
    selected = 0;
    offset = 0;
  }

  std::vector<std::string> filter() const
  {
    std::regex re(query);

    if (query.empty()) {
      return inputs;
    }
    else {
      std::vector<std::string> ret;
      for (auto&& elem : inputs) {
        if (std::regex_search(elem, re)) {
          ret.push_back(elem);
        }
      }
      return ret;
    }
  }

  void render_items()
  {
    termbox.clear();

    std::string query_str = query_header + query;
    termbox.println(0, query_str, TB_WHITE, TB_BLACK);
    termbox.putchar(query_str.length(), 0, ' ', TB_WHITE, TB_WHITE);

    for (int y = 0; y < (int)rendered.size(); ++y) {
      if (y == selected) {
        termbox.println(y + y_offset, rendered[y], TB_RED, TB_WHITE);
      }
      else {
        termbox.println(y + y_offset, rendered[y], TB_WHITE, TB_BLACK);
      }
    }

    termbox.flush();
  }
};

int main()
{
  std::regex ansi(R"(\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K])");

  std::vector<std::string> inputs;
  for (std::string line; std::getline(std::cin, line);) {
    inputs.push_back(std::regex_replace(line, ansi, ""));
  }

  bool selected;
  std::string selected_str;
  {
    auto cli = CocoClient{std::move(inputs)};
    std::tie(selected, selected_str) = cli.run();
  }

  if (selected) {
    std::cout << selected_str << std::endl;
  }
  return 0;
}
