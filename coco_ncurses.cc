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

bool is_utf8_first(uint8_t ch) {
  if ((ch & 0x80) == 0) {
    return true;  // ascii (0b0.......)
  }

  if ((ch & 0xC0) != 0x80) {
    return false; // 0b10......
  }

  if ((ch & 0xFE) == 0xFE) {
    return false; // 0b1111111.
  }

  return true;    // 0b11......
}

bool is_utf8_cont(uint8_t ch) {
  return (ch & 0xC0) == 0x80;
}

int get_utf8_char_length(uint8_t ch)
{
  if (!is_utf8_first(ch)) {
    return -1;
  }

	for (int i = 0; i < 6; ++i) {
		if ((ch & (0x01 << (7 - i))) == 0) {
	    return std::max(0, i - 1) + 1;
		}
	}

  return -2;
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


enum class Status {
  Selected, Escaped, Continue,
};

class Coco {
  std::vector<std::string> const& lines;
  std::string query;
  size_t cursor = 0;

public:
  Coco(std::vector<std::string> const& lines)
    : lines(lines)
  {
  }

  void render_screen();
  Status handle_key_event();

  std::string get_selection() const {
    return lines[cursor];
  }
};

void Coco::render_screen()
{
  ::werase(stdscr);

  std::string query_str = "QUERY> " + query;
  mvwaddstr(stdscr, 0, 0, query_str.c_str());

  for (int y = 0; y < lines.size(); ++y) {
    mvwaddstr(stdscr, y+1, 1, lines[y].c_str());
  }
  mvwaddstr(stdscr, cursor+1, 0, ">");

  move(0, query_str.length());

  ::wrefresh(stdscr);
}

Status Coco::handle_key_event()
{
  int ch = ::getch();
  if (ch == 10) { // 10 = enter
    return Status::Selected;
  }
  else if (ch == 27) {
    ::nodelay(stdscr, true);
    int ch = ::getch();
    if (ch == -1) { // Escape
      return Status::Escaped;
    }
    ::ungetch(ch);
    ::nodelay(stdscr, false);
    return Status::Continue;
  }
  else if (ch == KEY_UP) {
    if (cursor > 0) {
      cursor--;
    }
    return Status::Continue;
  }
  else if (ch == KEY_DOWN) {
    if (cursor < lines.size() - 1) {
      cursor++;
    }
    return Status::Continue;
  }
  else if (ch == 127) {  // 127 = backspace
    if (!query.empty()) {
      query.pop_back();
    }
    return Status::Continue;
  }
  else { // character
    query.push_back(ch);
    return Status::Continue;
  }

  throw std::logic_error{"unreachable"};
}

class Ncurses {
public:
  Ncurses() {
    ::initscr();
    ::noecho();
    ::cbreak();
    ::keypad(stdscr, true);
    ::ESCDELAY = 25;
  }
  ~Ncurses() {
    ::endwin();
  }
};

std::tuple<bool, std::string> do_selection(std::vector<std::string> const& lines)
{
  Ncurses ncurses;
  
  Coco coco{lines};

  coco.render_screen();
  while (true) {
    auto result = coco.handle_key_event();
    if (result == Status::Selected) {
      return std::make_tuple(true, coco.get_selection());
    } else if (result == Status::Escaped) {
      return std::make_tuple(false, ""s);
    }

    coco.render_screen();
  }

  throw std::logic_error{"unreachable"};
}

int main()
{
  std::setlocale(LC_ALL, "");

  try {
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
    return 0;
  }
  catch (std::exception& e) {
    std::cerr << "An error is thrown: " << e.what() << std::endl;
    return -1;
  }
}