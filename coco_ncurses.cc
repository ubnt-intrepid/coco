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

  if ((ch & 0xC0) == 0x80) {
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

size_t get_utf8_char_length(uint8_t ch)
{
  if (!is_utf8_first(ch)) {
    throw std::runtime_error(string(__FUNCTION__) + ": the first byte is not UTF8");
  }

	for (int i = 0; i < 6; ++i) {
		if ((ch & (0x01 << (7 - i))) == 0) {
	    return std::max(0, i - 1) + 1;
		}
	}

  throw std::runtime_error(string(__FUNCTION__) + ": unreachable");
}

std::string get_utf8_char() {
  std::array<uint8_t, 6> buf{0};

  auto ch0 = static_cast<uint8_t>(::getch() & 0x000000FF);
  size_t len = get_utf8_char_length(ch0);
  buf[0] = ch0;
  
  for (size_t i = 1; i < len; ++i) {
    auto ch = static_cast<uint8_t>(::getch() & 0x000000FF);
    if (!is_utf8_cont(ch)) {
      throw std::runtime_error(string(__FUNCTION__) + ": wrong byte exists");
    }
    buf[i] = ch;
  }

  return std::string(buf.data(), buf.data() + len);
}

void pop_back_utf8(std::string& str)
{
  if (str.empty())
    return;

  for (size_t len = str.size() - 1; len >= 0; --len) {
    if (!is_utf8_cont(str[len])) {
      str.resize(len);
      return;
    }
  }
}

class Coco {
  enum class Status {
    Selected, Escaped, Continue,
  };

  std::vector<std::string> const& lines;
  std::string query;
  size_t cursor = 0;

public:
  Coco(std::vector<std::string> const& lines)
    : lines(lines)
  {
  }

  std::tuple<bool, std::string> run();

private:
  void render_screen();
  Status handle_key_event();
};

void Coco::render_screen()
{
  std::string query_str = "QUERY> " + query;
 
  ::werase(stdscr);
 
 for (int y = 0; y < lines.size(); ++y) {
    mvwaddstr(stdscr, y+1, 1, lines[y].c_str());
  }
  mvwaddstr(stdscr, cursor+1, 0, ">");

  mvwaddstr(stdscr, 0, 0, query_str.c_str());

  ::wrefresh(stdscr);
}

auto Coco::handle_key_event() -> Status
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
      pop_back_utf8(query);
    }
    return Status::Continue;
  }
  else if (is_utf8_first(ch & 0xFF)) { // utf-8 character
    ::ungetch(ch);
    auto ch = get_utf8_char();
    query += ch;
    return Status::Continue;
  }

  throw std::logic_error(std::string(__FUNCTION__) + ": unreachable");
}

std::tuple<bool, std::string> Coco::run()
{
  render_screen();
  
  while (true) {
    auto result = handle_key_event();
    if (result == Status::Selected) {
      return std::make_tuple(true, lines[cursor]);
    } else if (result == Status::Escaped) {
      break;
    }
    render_screen();
  }
  
  return std::make_tuple(false, ""s);
}


class Ncurses {
public:
  Ncurses() {
    freopen("/dev/tty", "rw", stdin);
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
    {
      Ncurses ncurses;
      Coco coco{lines};
      std::tie(is_selected, selection) = coco.run();
    }

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