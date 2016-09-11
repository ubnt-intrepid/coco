#include <ncurses.h>
#include <locale>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <regex>
#include <iostream>
#include <stdexcept>
using namespace std::literals::string_literals;
using std::string;
using std::tuple;
using std::vector;


int get_utf8_char_length(uint8_t ch)
{
  if ((ch & 0xC0) == 0x80) {
    return -1;
  }
  else if ((ch & 0xFE) == 0xFE) {
    return -2;
  }

	for (int i = 0; i < 6; ++i) {
		if ((ch & (0x01 << (7 - i))) == 0) {
	    return std::max(0, i - 1) + 1;
		}
	}
  return -3;
}

bool is_utf8_cont(uint8_t ch) {
  return (ch & 0xC0) == 0x80;
}

std::string get_utf8_char() {
  std::array<int8_t, 6> buf{0};
  std::array<int, 6> rawbuf{0};

  // get the number of bytes of UTF8 character.
  int ch0_raw = ::getch();
  int8_t ch0 = static_cast<int8_t>(ch0_raw & 0x000000FF);
  int len = get_utf8_char_length(ch0);
  if (len < 0) {
    ::ungetch(ch0_raw);
    throw std::runtime_error(string(__FUNCTION__) + ": the first byte is not UTF8");
  }
  buf[0] = ch0;
  rawbuf[0] = ch0_raw;

  for (int i = 1; i < len; ++i) {
    int ch_raw = ::getch();
    int8_t ch = static_cast<int8_t>(ch_raw & 0x000000FF);
    if (!is_utf8_cont(ch)) {
      ::ungetch(ch_raw);
      for (int k = i - 1; k >= 0; --k) {
        ::ungetch(rawbuf[k]);
      }
      throw std::runtime_error(string(__FUNCTION__) + ": wrong byte exists");
    }
    buf[i] = ch;
    rawbuf[i] = ch_raw;
  }

  return std::string(buf.data(), buf.data() + len);
}


class Coco {
  std::vector<std::string> const& lines;
  std::string query;

public:
  Coco(std::vector<std::string> const& lines)
    : lines(lines)
  {
  }

  void render_screen();
  bool handle_key_event();
};

void Coco::render_screen()
{
  ::wclear(stdscr);

  ::wmove(stdscr, 0, 0);
  ::waddstr(stdscr, query.c_str());
  ::wrefresh(stdscr);
}

bool Coco::handle_key_event()
{
  int ch = ::getch();
  if (ch == 10) { // 10 = enter
    return false;
  }
  else if (ch == 27) {
    ::nodelay(stdscr, true);
    int ch = ::getch();
    if (ch == -1) { // Escape
      return false;
    }
    ::ungetch(ch);
    ::nodelay(stdscr, false);
  }

  else if (ch == KEY_UP) {
    return true;
  }
  else if (ch == KEY_DOWN) {
    return true;
  }
  else if (ch == 127) {  // 127 = backspace
    query = ""s;
    return true;
  }
  else {  // normal string
    ::ungetch(ch);
    query = get_utf8_char();
    return true;
  }

  throw std::logic_error{"unreachable"};
}

std::tuple<bool, std::string> do_selection(std::vector<std::string> const& lines)
{
  ::initscr();
  ::noecho();
  ::cbreak();
  ::keypad(stdscr, true);
  ::ESCDELAY = 25;

  Coco coco{lines};

  coco.render_screen();
  while (coco.handle_key_event()) {
    coco.render_screen();
  }

  ::endwin();
  return std::make_tuple(false, ""s);
}

int main()
{
  std::setlocale(LC_ALL, "");

  // read lines from stdin.
  std::regex ansi(R"(\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K])");
  std::vector<std::string> lines;
  for (std::string line; std::getline(std::cin, line);) {
    lines.push_back(std::regex_replace(line, ansi, ""));
  }

  bool is_selected;
  std::string selection;
  std::tie(is_selected, selection) = do_selection(lines);

  if (is_selected) {
    std::cout << selection << std::endl; 
  }
}
