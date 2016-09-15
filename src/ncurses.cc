#include "ncurses.hh"

#include <ncurses.h>
#include <stdexcept>
#include "utf8.hh"

namespace {
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
}

Ncurses::Ncurses()
{
  // create a new terminal
  tty_in.reset(fopen("/dev/tty", "r"));
  tty_out.reset(fopen("/dev/tty", "w"));
  screen = ::newterm(getenv("TERM"), tty_out.get(), tty_in.get());

  // switch to the current screen.
  screen_orig = ::set_term(screen);

  // setup
  ::noecho();             // do not echo back characters
  ::cbreak();             // without buffering
  ::keypad(stdscr, true); // convert escape sequeces to key code
  ::ESCDELAY = 25;        // set delay time

  // initialize colormap.
  // start_color();
  // ::init_pair(1, COLOR_RED, COLOR_WHITE);
}

Ncurses::~Ncurses()
{
  // reset current screen.
  ::endwin();

  // switch to original screen.
  ::set_term(screen_orig);

  // release all resources of current session.
  ::delscreen(screen);
}

void Ncurses::erase() { ::werase(stdscr); }

void Ncurses::refresh() { ::wrefresh(stdscr); }

std::tuple<int, int> Ncurses::get_size() const
{
  int width, height;
  getmaxyx(stdscr, height, width);
  return std::make_tuple(width, height);
}

void Ncurses::add_str(int x, int y, std::string const& text) { mvwaddstr(stdscr, y, x, text.c_str()); }

void Ncurses::change_attr(int x, int y, int n, int col)
{
  // attrset(COLOR_PAIR(col));
  mvwchgat(stdscr, y, x, n, A_BOLD | A_UNDERLINE, col, nullptr);
  // attrset(COLOR_PAIR(0));
}

Event Ncurses::poll_event()
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
      return Event{Key::Alt, ch};
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
  else if (ch == 9) {
    return Event{Key::Tab};
  }
  else if (ch == 127) {
    return Event{Key::Backspace};
  }
  else if (ch == 18) {
    return Event{Key::Ctrl, 'r'};
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
