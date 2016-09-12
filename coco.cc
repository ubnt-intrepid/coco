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

bool is_utf8_first(uint8_t ch)
{
  return (ch & 0xC0) != 0x80 && (ch & 0xFE) != 0xFE && ((ch & 0x80) == 0 || (ch & 0xC0) == 0xC0);
}

bool is_utf8_cont(uint8_t ch) { return (ch & 0xC0) == 0x80; }

size_t get_utf8_char_length(uint8_t ch)
{
  if (!is_utf8_first(ch)) {
    throw std::runtime_error(string(__FUNCTION__) + ": the first byte is not UTF8");
  }

  for (int i = 0; i < 6; ++i) {
    if ((ch & (0x01 << (7 - i))) == 0) {
      return std::max(1, i);
    }
  }

  throw std::runtime_error(string(__FUNCTION__) + ": unreachable");
}

std::string get_utf8_char()
{
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
    Selected,
    Escaped,
    Continue,
  };

  std::vector<std::string> const& lines;
  std::vector<std::string> filtered;
  std::string query;
  size_t cursor = 0;
  size_t offset = 0;

public:
  Coco(std::vector<std::string> const& lines) : lines(lines), filtered(lines) {}

  std::tuple<bool, std::string> run();

private:
  void render_screen();
  Status handle_key_event();

  void filter_by_query();
};

void Coco::render_screen()
{
  std::string query_str = "QUERY> " + query;

  ::werase(stdscr);

  int width, height;
  getmaxyx(stdscr, height, width);

  for (size_t y = 0; y < std::min<size_t>(filtered.size() - offset, width - 1); ++y) {
    mvwaddstr(stdscr, y + 1, 0, filtered[y + offset].c_str());
    if (y == cursor) {
      attrset(COLOR_PAIR(2));
      mvwchgat(stdscr, y + 1, 0, -1, A_NORMAL, 2, nullptr);
      attrset(COLOR_PAIR(1));
    }
  }

  mvwaddstr(stdscr, 0, 0, query_str.c_str());

  ::wrefresh(stdscr);
}

auto Coco::handle_key_event() -> Status
{
  int ch = ::getch();
  if (ch == 10) { // 10 = enter
    return filtered.size() > 0 ? Status::Selected : Status::Escaped;
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
    if (cursor == 0) {
      offset = std::max(0, (int)offset - 1);
    }
    else {
      cursor--;
    }
    return Status::Continue;
  }
  else if (ch == KEY_DOWN) {
    int width, height;
    getmaxyx(stdscr, height, width);

    if (cursor == height - 2) {
      offset = std::min<size_t>(offset + 1, std::max<int>(0, filtered.size() - height + 1));
    }
    else {
      cursor = std::min<size_t>(cursor + 1, std::min<size_t>(filtered.size() - offset, height - 1) - 1);
    }
    return Status::Continue;
  }
  else if (ch == 127) { // 127 = backspace
    if (!query.empty()) {
      pop_back_utf8(query);
      filter_by_query();
    }
    return Status::Continue;
  }
  else if (is_utf8_first(ch & 0xFF)) { // utf-8 character
    ::ungetch(ch);
    auto ch = get_utf8_char();
    query += ch;
    filter_by_query();
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
      return std::make_tuple(true, filtered[cursor]);
    }
    else if (result == Status::Escaped) {
      break;
    }
    render_screen();
  }

  return std::make_tuple(false, ""s);
}

void Coco::filter_by_query()
{
  cursor = 0;
  offset = 0;

  if (query.empty()) {
    filtered = lines;
  }
  else {
    std::regex re(query);
    filtered.resize(0);
    for (auto&& line : lines) {
      if (std::regex_search(line, re)) {
        filtered.push_back(line);
      }
    }
  }
}

class Ncurses {
public:
  Ncurses()
  {
    ::initscr();
    ::noecho();
    ::cbreak();
    ::keypad(stdscr, true);
    ::ESCDELAY = 25;

    start_color();
    ::init_pair(1, COLOR_WHITE, COLOR_BLACK);
    ::init_pair(2, COLOR_RED, COLOR_WHITE);
  }
  ~Ncurses() { ::endwin(); }
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

    freopen("/dev/tty", "r", stdin);
    freopen("/dev/tty", "w", stdout);

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
