#include <iostream>
#include <termbox.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <array>

static const std::string chars{"nnnnnnnnnbbbbbbbbbuuuuuuuuuBBBBBBBBB"};

static constexpr std::array<std::uint16_t, 4> all_attrs = {
    0, TB_BOLD, TB_UNDERLINE, TB_BOLD | TB_UNDERLINE,
};

static int next_char(int current)
{
  current++;
  if (!chars[current])
    current = 0;
  return current;
}

static void draw_line(int x, int y, std::uint16_t bg)
{
  int current_char = 0;
  for (int a = 0; a < 4; ++a) {
    for (int c = TB_DEFAULT; c <= TB_WHITE; c++) {
      std::uint16_t fg = all_attrs[a] | c;
      tb_change_cell(x, y, chars[current_char], fg, bg);
      current_char = next_char(current_char);
      x++;
    }
  }
}

template <std::size_t N> static void print_combinations_table(int sx, int sy, std::array<std::uint16_t, N> const& attrs)
{
  for (int i = 0; i < attrs.size(); ++i) {
    for (int c = TB_DEFAULT; c <= TB_WHITE; ++c) {
      std::uint16_t bg = attrs[i] | c;
      draw_line(sx, sy, bg);
      ++sy;
    }
  }
}

static void draw_all()
{
  tb_clear();

  tb_select_output_mode(TB_OUTPUT_NORMAL);

  static std::array<std::uint16_t, 2> col1 = {0, TB_BOLD};
  static std::array<std::uint16_t, 1> col2 = {TB_REVERSE};

  print_combinations_table(1, 1, col1);
  print_combinations_table(2 + chars.length(), 1, col2);
  tb_present();

  tb_select_output_mode(TB_OUTPUT_GRAYSCALE);
  int x, y;
  for (x = 0, y = 23; x < 24; ++x) {
    tb_change_cell(x, y, '@', x, 0);
    tb_change_cell(x + 25, y, ' ', 0, x);
  }
  tb_present();

  tb_select_output_mode(TB_OUTPUT_216);
  y++;
  for (int c = 0, x = 0; c < 216; ++c, ++x) {
    if (!(x % 24)) {
      x = 0;
      ++y;
    }
    tb_change_cell(x, y, '@', c, 0);
    tb_change_cell(x + 25, y, ' ', 0, c);
  }
  tb_present();

  tb_select_output_mode(TB_OUTPUT_256);
  y++;
  for (int c = 0, x = 0; c < 256; ++c, ++x) {
    if (!(x % 24)) {
      x = 0;
      ++y;
    }
    tb_change_cell(x, y, '+', c | ((y & 1) ? TB_UNDERLINE : 0), 0);
    tb_change_cell(x + 25, y, ' ', 0, c);
  }
  tb_present();
}

int main(int argc, char* argv[])
{
  static_cast<void>(argc);
  static_cast<void>(argv);

  if (int error = tb_init()) {
    std::cerr << "tb_init() failed with error code " << error << std::endl;
    return error;
  }

  draw_all();

  for (tb_event ev; tb_poll_event(&ev);) {
    if (ev.type == TB_EVENT_KEY && ev.key == TB_KEY_ESC) {
      break;
    }
    else if (ev.type == TB_EVENT_RESIZE) {
      draw_all();
    }
  }

  tb_shutdown();
  return 0;
}
