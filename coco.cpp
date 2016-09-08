#include <termbox.h>
#include <cstdint>
#include <nanojson.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <regex>

class TermBox {
public:
  TermBox() = default;
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
  std::vector<std::string> inputs, render_items, filtered;
  std::string query, selected_str;
  long cursor, selected;
  size_t offset;

  std::string const query_header = "QUERY> ";
  static constexpr unsigned y_offset = 1;

public:
  CocoClient()
  {
    for (std::string line; std::getline(std::cin, line);) {
      inputs.push_back(line);
    }
    termbox.init();
    apply_filter();
  }

  std::string select_line()
  {
    while (true) {
      update_items();
      if (handle_event())
        break;
    }

    return selected_str;
  }

  bool handle_event()
  {
    tb_event ev;
    tb_poll_event(&ev);

    if (ev.key == TB_KEY_ENTER) {
      selected_str = render_items[offset + selected];
      return true;
    }
    else if (ev.key == TB_KEY_ESC) {
      return true;
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

          render_items.resize(filtered.size() - offset);
          std::copy(filtered.begin() + offset, filtered.end(), render_items.begin());
        }
      }
    }
    else if (ev.key == TB_KEY_ARROW_DOWN) {
      if (cursor < render_items.size() - 1) {
        cursor += 1;
      }
      if ((render_items.size() < tb_height() - 1) && (selected < render_items.size() - 1)) {
        selected += 1;
      }
      else if ((render_items.size() > tb_height() - 1) && (selected < tb_height() - 1)) {
        selected += 1;
      }

      if (selected == tb_height() - 1) {
        selected -= 1;
        if (offset < filtered.size() - 1) {
          offset += 1;

          render_items.resize(filtered.size() - offset);
          std::copy(filtered.begin() + offset, filtered.end(), render_items.begin());
        }
      }
    }
    else if ((ev.ch != 0) || (ev.key == 32 && ev.ch == 0)) {
      query.push_back(ev.ch);
      apply_filter();
    }
    return false;
  }

private:
  void apply_filter()
  {
    filtered = filter();
    render_items = filtered;
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
    tb_clear();

    print_query();
    for (int y = 0; y < (int)render_items.size(); ++y) {
      print_line(render_items[y], y, y == selected);
    }

    tb_present();
  }

  void print_query() const
  {
    std::string query_str = query_header + query;

    for (int x = 0; x < tb_width(); ++x) {
      auto const c = static_cast<uint32_t>(x < query_str.length() ? query_str[x] : ' ');
      if (x == query_str.length()) {
        tb_change_cell(x, 0, c, TB_WHITE, TB_WHITE);
      }
      else {
        tb_change_cell(x, 0, c, TB_WHITE, TB_BLACK);
      }
    }
  }

  void print_line(std::string line, unsigned y, bool selected) const
  {
    for (int x = 0; x < tb_width(); ++x) {
      auto const c = static_cast<uint32_t>(x < line.length() ? line[x] : ' ');
      if (selected) {
        tb_change_cell(x, y + y_offset, c, TB_RED, TB_WHITE);
      }
      else {
        tb_change_cell(x, y + y_offset, c, TB_WHITE, TB_BLACK);
      }
    }
  }
};

int main()
{
  std::string selected;
  {
    CocoClient cli{};
    selected = cli.select_line();
  }

  if (!selected.empty()) {
    std::cout << selected << std::endl;
  }
  return 0;
}
