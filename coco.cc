#include <ncurses.h>
#include <locale>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <regex>
#include <iostream>
#include <stdexcept>
#include <nanojson.hpp>
#include <cmdline.h>
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

  for (ssize_t len = str.size() - 1; len >= 0; --len) {
    if (!is_utf8_cont(str[len])) {
      str.resize(len);
      return;
    }
  }
}

enum class Key { Enter, Esc, Alt, Up, Down, Left, Right, Backspace, Char, Unknown };

class Event {
  Key key;
  int mod;
  std::string ch;

public:
  Event(Key key) : key{key}, mod{0}, ch{} {}
  Event(int mod) : key{Key::Alt}, mod{mod}, ch{} {}
  Event(std::string&& ch) : key{Key::Char}, mod{0}, ch{ch} {}

  std::string const& as_chars() const { return ch; }

  inline bool operator==(Key key) const { return this->key == key; }
};

Event poll_event()
{
  int ch = ::getch();
  if (ch == 10) {
    return Event{Key::Enter};
  }
  else if (ch == 27) {
    ::nodelay(stdscr, true);
    int ch = ::getch();
    if (ch == -1) {
      ::nodelay(stdscr, false);
      return Event{Key::Esc};
    }
    else {
      ::nodelay(stdscr, false);
      return Event{ch};
    }
  }
  else if (ch == KEY_UP) {
    return Event{Key::Up};
  }
  else if (ch == KEY_DOWN) {
    return Event{Key::Down};
  }
  else if (ch == KEY_LEFT) {
    return Event{Key::Left};
  }
  else if (ch == KEY_RIGHT) {
    return Event{Key::Right};
  }
  else if (ch == 127) {
    return Event{Key::Backspace};
  }
  else if (is_utf8_first(ch & 0xFF)) {
    ::ungetch(ch);
    auto ch = get_utf8_char();
    return Event{std::move(ch)};
  }
  else {
    return Event{Key::Unknown};
  }
}

// wrapper of Ncurses API.
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

struct Config {
  std::string prompt;
  size_t y_offset;

public:
  void read_from(int argc, char const** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
  }
};

// represents a instance of Coco client.
class Coco {
  enum class Status {
    Selected,
    Escaped,
    Continue,
  };

  Config config;

  std::vector<std::string> const& lines;
  std::vector<std::string> filtered;
  std::string query;
  size_t cursor = 0;
  size_t offset = 0;

public:
  Coco(Config const& config, std::vector<std::string> const& lines) : config(config), lines(lines), filtered(lines) {}

  std::tuple<bool, std::string> select_line(Ncurses ncurses);

private:
  void render_screen();
  Status handle_key_event(Event const& ev);
  void update_filter_list();
  std::vector<std::string> filter_by_regex(std::vector<std::string> const& lines) const;
};

void Coco::render_screen()
{
  std::string query_str = config.prompt + query;

  ::werase(stdscr);

  int width, height;
  getmaxyx(stdscr, height, width);
  static_cast<void>(height);

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

auto Coco::handle_key_event(Event const& ev) -> Status
{
  if (ev == Key::Enter) {
    return filtered.size() > 0 ? Status::Selected : Status::Escaped;
  }
  else if (ev == Key::Esc) {
    return Status::Escaped;
  }
  else if (ev == Key::Up) {
    if (cursor == 0) {
      offset = std::max(0, (int)offset - 1);
    }
    else {
      cursor--;
    }
    return Status::Continue;
  }
  else if (ev == Key::Down) {
    int width, height;
    getmaxyx(stdscr, height, width);

    if (cursor == static_cast<size_t>(height - 1 - config.y_offset)) {
      offset = std::min<size_t>(offset + 1, std::max<int>(0, filtered.size() - height + config.y_offset));
    }
    else {
      cursor = std::min<size_t>(cursor + 1, std::min<size_t>(filtered.size() - offset, height - config.y_offset) - 1);
    }
    return Status::Continue;
  }
  else if (ev == Key::Backspace) {
    if (!query.empty()) {
      pop_back_utf8(query);
      update_filter_list();
    }
    return Status::Continue;
  }
  else if (ev == Key::Char) {
    query += ev.as_chars();
    update_filter_list();
    return Status::Continue;
  }
  else {
    return Status::Continue;
  }
}

std::tuple<bool, std::string> Coco::select_line(Ncurses ncurses)
{
  static_cast<void>(ncurses);

  render_screen();

  while (true) {
    Event ev = poll_event();

    auto result = handle_key_event(ev);
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

void Coco::update_filter_list()
{
  filtered = filter_by_regex(lines);
  cursor = 0;
  offset = 0;
}

std::vector<std::string> Coco::filter_by_regex(std::vector<std::string> const& lines) const
{
  if (query.empty()) {
    return lines;
  }
  else {
    std::regex re(query);
    std::vector<std::string> filtered;
    for (auto&& line : lines) {
      if (std::regex_search(line, re)) {
        filtered.push_back(line);
      }
    }
    return filtered;
  }
}

int main(int argc, char const* argv[])
{
  std::setlocale(LC_ALL, "");

  try {
    // read lines from stdin.
    std::regex ansi(R"(\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K])");
    std::vector<std::string> lines;
    for (std::string line; std::getline(std::cin, line);) {
      lines.push_back(std::regex_replace(line, ansi, ""));
    }

    // Initialize Coco application.
    Config config;
    config.prompt = "QUERY> ";
    config.y_offset = 1;
    config.read_from(argc, argv);

    Coco coco{config, lines};

    // reopen file handlers of TTY for Ncurses session.
    freopen("/dev/tty", "r", stdin);
    freopen("/dev/tty", "w", stdout);

    // retrieve a selection from lines.
    // note that it ensures to shutdown ncurses when returned.
    bool is_selected;
    std::string selection;
    std::tie(is_selected, selection) = coco.select_line(Ncurses{});

    // show selected line.
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
