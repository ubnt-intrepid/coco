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


class CocoClient {
  TermBox termbox;

  // states
  std::vector<std::string> inputs, filtered, rendered;
  std::string query;
  long cursor, selected;
  size_t offset;

  // configurations
  std::string const query_header = "QUERY> ";
  static constexpr unsigned y_offset = 1;

public:
  CocoClient(std::vector<std::string> inputs) : inputs{std::move(inputs)} {}

  std::string select_line()
  {
    apply_filter();

    while (true) {
      render();
      Event ev = termbox.poll_event();

      bool quit;
      std::string selected_str;
      std::tie(quit, selected_str) = handle_event(ev);
      if (quit) {
        return selected_str;
      }
    }

    return {};
  }

  std::tuple<bool, std::string> handle_event(Event const& ev)
  {
    if (ev.is_key(TB_KEY_ENTER)) {
      return std::make_tuple(true, filtered.empty() ? std::string{} : rendered[offset + selected]);
    }
    else if (ev.is_key(TB_KEY_ESC)) {
      return std::make_tuple(true, std::string{});
    }
    else if (ev.is_key(TB_KEY_BACKSPACE) || ev.is_key(TB_KEY_BACKSPACE2)) {
      if (!query.empty()) {
        query.pop_back();
        apply_filter();
      }
    }
    else if (ev.is_key(TB_KEY_ARROW_UP)) {
      if (selected > -1) {
        selected -= 1;
      }
      if (cursor > 0) {
        cursor -= 1;
      }

      if (selected == -1) {
        selected += 1;
        if (offset > 0) {
          offset -= 1;
          rendered.assign(filtered.begin() + offset, filtered.end());
        }
      }
    }
    else if (ev.is_key(TB_KEY_ARROW_DOWN)) {
      if (cursor < rendered.size() - 1) {
        cursor += 1;
      }
      if ((rendered.size() < termbox.height() - 1) && (selected < rendered.size() - 1)) {
        selected += 1;
      }
      else if ((rendered.size() > termbox.height() - 1) && (selected < termbox.height() - 1)) {
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
    else if (ev.is_char()) {
      query.push_back(ev.as_char());
      apply_filter();
    }

    return std::make_tuple(false, std::string{});
  }

private:
  void apply_filter()
  {
    filtered = filter();
    rendered = filtered;
    selected = 0;
    cursor = 0;
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

  void render()
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

  std::string selected;
  {
    auto cli = CocoClient{std::move(inputs)};
    selected = cli.select_line();
  }

  if (!selected.empty()) {
    std::cout << selected << std::endl;
  }
  return 0;
}
