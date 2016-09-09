#include <termbox.h>
#include <nanojson.hpp>

#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

class TermBox {
public:
  TermBox() { init(); }

  ~TermBox() noexcept
  {
    try {
      tb_shutdown();
    }
    catch (...) {
    }
  }

  void init()
  {
    int error = tb_init();
    if (error) {
      std::cerr << "tb_init() failed with error code " << error << std::endl;
      std::exit(error);
    }
  }
};

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
      update_items();

      tb_event ev;
      tb_poll_event(&ev);

      bool quit;
      std::string selected_str;
      std::tie(quit, selected_str) = handle_event(ev);
      if (quit) {
        return selected_str;
      }
    }

    return {};
  }

  std::tuple<bool, std::string> handle_event(tb_event& ev)
  {
    if (ev.key == TB_KEY_ENTER) {
      return std::make_tuple(true, filtered.empty() ? std::string{} : rendered[offset + selected]);
    }
    else if (ev.key == TB_KEY_ESC) {
      return std::make_tuple(true, std::string{});
    }
    else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) {
      if (!query.empty()) {
        query.pop_back();
        apply_filter();
      }
    }
    else if (ev.key == TB_KEY_ARROW_UP) {
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
    else if (ev.key == TB_KEY_ARROW_DOWN) {
      if (cursor < rendered.size() - 1) {
        cursor += 1;
      }
      if ((rendered.size() < tb_height() - 1) && (selected < rendered.size() - 1)) {
        selected += 1;
      }
      else if ((rendered.size() > tb_height() - 1) && (selected < tb_height() - 1)) {
        selected += 1;
      }

      if (selected == tb_height() - 1) {
        selected -= 1;
        if (offset < filtered.size() - 1) {
          offset += 1;
          rendered.assign(filtered.begin() + offset, filtered.end());
        }
      }
    }
    else if ((ev.ch != 0) || (ev.key == 32 && ev.ch == 0)) {
      query.push_back(ev.ch);
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

  void update_items() const
  {
    std::string query_str = query_header + query;

    tb_clear();

    print_line(0, query_str, TB_WHITE, TB_BLACK);
    tb_change_cell(query_str.length(), 0, ' ', TB_WHITE, TB_WHITE);

    for (int y = 0; y < (int)rendered.size(); ++y) {
      if (y == selected) {
        print_line(y + y_offset, rendered[y], TB_RED, TB_WHITE);
      }
      else {
        print_line(y + y_offset, rendered[y], TB_WHITE, TB_BLACK);
      }
    }

    tb_present();
  }

  void print_line(unsigned y, std::string const& line, std::uint16_t fg, std::uint16_t bg) const
  {
    for (int x = 0; x < tb_width(); ++x) {
      auto const ch = static_cast<uint32_t>(x < line.length() ? line[x] : ' ');
      tb_change_cell(x, y, ch, fg, bg);
    }
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
