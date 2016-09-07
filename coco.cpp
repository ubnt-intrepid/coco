#include <termbox.h>
#include <cstdint>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <regex>

class CocoClient {
  std::vector<std::string> inputs;
  std::vector<std::string> render_items;
  std::vector<std::string> filtered;

  std::string query;

  long selected;
  long cursor;

  size_t offset;
  size_t query_offset;

  bool flag_selected = false;

public:
  CocoClient()
  {
    for (std::string line; std::getline(std::cin, line);) {
      inputs.push_back(line);
    }

    if (int error = tb_init()) {
      std::cerr << "tb_init() failed with error code " << error << std::endl;
      std::exit(error);
    }

    tb_clear();
    update_query();
    init();
  }

  ~CocoClient()
  {
    try {
      tb_shutdown();

      if (flag_selected) {
        std::cout << render_items[offset + selected] << std::endl;
      }
    }
    catch (...) {
    }
  }

  bool poll_event()
  {
    update_items();

    tb_event ev;
    tb_poll_event(&ev);

    if (ev.key == TB_KEY_ENTER) {
      flag_selected = true;
      return true;
    }
    else if (ev.key == TB_KEY_ESC) {
      return true;
    }
    else if (ev.key == TB_KEY_BACKSPACE) {
      if (!query.empty()) {
        query.pop_back();
        update_query();
        init();
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
    else {
      if ((ev.ch != 0) || (ev.key == 32 && ev.ch == 0)) {
        query.push_back(ev.ch);
        update_query();
        init();
      }
    }
    return false;
  }

private:
  std::vector<std::string> filter()
  {
    std::regex re(query);

    if (query.empty()) {
      return inputs;
    }
    else {
      std::vector<std::string> ret;
      for (auto&& elem : inputs) {
        if (std::regex_match(elem, re)) {
          ret.push_back(elem);
        }
      }
      return ret;
    }
  }

  void update_query()
  {
    std::string const query_header = "QUERY> ";
    std::string print_query = query_header + query;

    for (int x = 0; x < tb_width(); ++x) {
      auto const c = static_cast<uint32_t>(x < print_query.length() ? print_query[x] : ' ');
      if (x == print_query.length()) {
        tb_change_cell(x, 0, c, TB_WHITE, TB_WHITE);
      }
      else {
        tb_change_cell(x, 0, c, TB_WHITE, TB_BLACK);
      }
    }
  }

  void init()
  {
    filtered = filter();
    render_items = filtered;
    selected = 0;
    cursor = 0;
    offset = 0;
  }

  void print(unsigned x, unsigned y, std::string line, bool selected)
  {
    auto const c = static_cast<uint32_t>(x < line.length() ? line[x] : ' ');
    constexpr unsigned y_offset = 1;

    if (selected) {
      tb_change_cell(x, y + y_offset, c, TB_RED, TB_WHITE);
    }
    else {
      tb_change_cell(x, y + y_offset, c, TB_WHITE, TB_BLACK);
    }
  }

  void update_items()
  {
    tb_clear();
    update_query();

    for (int y = 0; y < (int)render_items.size(); ++y) {
      for (int x = 0; x < tb_width(); ++x) {
        print(x, y, render_items[y], y == selected);
      }
    }

    tb_present();
  }
};

int main()
{
  CocoClient cli{};
  while (!cli.poll_event())
    ;
  return 0;
}
