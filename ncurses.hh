#ifndef __HEADER_NCURSE__
#define __HEADER_NCURSE__

#include <ncurses.h>
#include <string>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include "utf8.hh"

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

// wrapper of Ncurses API.
class Ncurses {
  struct deleter_t {
    void operator()(FILE* fd) { ::fclose(fd); }
  };

  std::unique_ptr<FILE, deleter_t> tty_in, tty_out;

public:
  Ncurses()
  {
    tty_in.reset(fopen("/dev/tty", "r"));
    tty_out.reset(fopen("/dev/tty", "w"));
    ::newterm(getenv("TERM"), tty_out.get(), tty_in.get());

    ::noecho();
    ::cbreak();
    ::keypad(stdscr, true);
    ::ESCDELAY = 25;

    start_color();
    ::init_pair(1, COLOR_WHITE, COLOR_BLACK);
    ::init_pair(2, COLOR_RED, COLOR_WHITE);
  }

  Ncurses(Ncurses&&) noexcept = default;

  ~Ncurses() { ::endwin(); }

  void erase() { ::werase(stdscr); }

  void refresh() { ::wrefresh(stdscr); }

  std::tuple<int, int> get_width_height() const
  {
    int width, height;
    getmaxyx(stdscr, height, width);
    return std::make_tuple(width, height);
  }

  void add_string(int x, int y, std::string const& text) { mvwaddstr(stdscr, y, x, text.c_str()); }

  void change_attr(int x, int y, int n, int col)
  {
    attrset(COLOR_PAIR(col));
    mvwchgat(stdscr, y, x, n, A_NORMAL, col, nullptr);
    attrset(COLOR_PAIR(1));
  }

  Event poll_event()
  {
    int ch = ::wgetch(stdscr);
    if (ch == 10) {
      return Event{Key::Enter};
    }
    else if (ch == 27) {
      ::nodelay(stdscr, true);
      int ch = ::wgetch(stdscr);
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

private:
  std::string get_utf8_char()
  {
    std::array<uint8_t, 6> buf{0};

    auto ch0 = static_cast<uint8_t>(::wgetch(stdscr) & 0x000000FF);
    size_t len = get_utf8_char_length(ch0);
    buf[0] = ch0;

    for (size_t i = 1; i < len; ++i) {
      auto ch = static_cast<uint8_t>(::wgetch(stdscr) & 0x000000FF);
      if (!is_utf8_cont(ch)) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": wrong byte exists");
      }
      buf[i] = ch;
    }

    return std::string(buf.data(), buf.data() + len);
  }
};

#endif