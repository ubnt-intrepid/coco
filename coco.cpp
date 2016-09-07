#include <termbox.h>
#include <cstdint>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <regex>

struct Env {
  std::vector<std::string> inputs;
  std::vector<std::string> render_items;
  std::vector<std::string> filtered;
  std::string query;
  long selected;
  long cursor;
  size_t offset;
  size_t query_offset;
};

static Env env;

std::vector<std::string> filter()
{
  std::regex re(env.query);

  if (env.query.empty()) {
    return env.inputs;
  }
  else {
    std::vector<std::string> ret;
    for (auto&& elem : env.inputs) {
      if (std::regex_match(elem, re)) {
        ret.push_back(elem);
      }
    }
    return ret;
  }
}

void update_query(bool init = false)
{
  std::string const query_header = "QUERY> ";

  std::string print_query = query_header + env.query;

  int const width = tb_width();

  for (int x = 0; x < width; ++x) {
    auto const c = static_cast<uint32_t>(x < print_query.length() ? print_query[x] : ' ');

    if (x == print_query.length()) {
      tb_change_cell(x, 0, c, TB_WHITE, TB_WHITE);
    }
    else {
      tb_change_cell(x, 0, c, TB_WHITE, TB_BLACK);
    }
  }

  if (init) {
    env.filtered = filter();
    env.render_items = env.filtered;
    env.selected = 0;
    env.cursor = 0;
    env.offset = 0;
  }
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

  for (int y = 0; y < (int)env.render_items.size(); ++y) {
    for (int x = 0; x < tb_width(); ++x) {
      print(x, y, env.render_items[y], y == env.selected);
    }
  }

  tb_present();
}

int main()
{
  for (std::string line; std::getline(std::cin, line);) {
    env.inputs.push_back(line);
  }

  if (int error = tb_init()) {
    std::cerr << "tb_init() failed with error code " << error << std::endl;
    return error;
  }
  tb_clear();

  update_query(true);

  bool selected = false;
  for (bool quit = false; !quit;) {

    update_items();

    tb_event ev;
    tb_poll_event(&ev);

    if (ev.key == TB_KEY_ENTER) {
      selected = true;
      quit = true;
    }
    else if (ev.key == TB_KEY_ESC) {
      quit = true;
    }
    else if (ev.key == TB_KEY_BACKSPACE) {
      if (!env.query.empty()) {
        env.query.pop_back();
        update_query(true);
      }
    }
    else if (ev.key == TB_KEY_ARROW_UP) {
      if (env.selected > -1) {
        env.selected -= 1;
      }
      if (env.cursor > 0) {
        env.cursor -= 1;
      }

      if (env.selected == -1) {
        env.selected += 1;
        if (env.offset > 0) {
          env.offset -= 1;

          env.render_items.resize(env.filtered.size() - env.offset);
          std::copy(env.filtered.begin() + env.offset, env.filtered.end(), env.render_items.begin());
        }
      }
    }
    else if (ev.key == TB_KEY_ARROW_DOWN) {
      if (env.cursor < env.render_items.size() - 1) {
        env.cursor += 1;
      }
      if ((env.render_items.size() < tb_height() - 1) && (env.selected < env.render_items.size() - 1)) {
        env.selected += 1;
      }
      else if ((env.render_items.size() > tb_height() - 1) && (env.selected < tb_height() - 1)) {
        env.selected += 1;
      }

      if (env.selected == tb_height() - 1) {
        env.selected -= 1;
        if (env.offset < env.filtered.size() - 1) {
          env.offset += 1;

          env.render_items.resize(env.filtered.size() - env.offset);
          std::copy(env.filtered.begin() + env.offset, env.filtered.end(), env.render_items.begin());
        }
      }
    }
    else {
      if ((ev.ch != 0) || (ev.key == 32 && ev.ch == 0)) {
        env.query.push_back(ev.ch);
        update_query(true);
      }
    }
  }

  tb_shutdown();

  if (selected) {
    std::cout << env.render_items[env.offset + env.selected] << std::endl;
  }

  return 0;
}
