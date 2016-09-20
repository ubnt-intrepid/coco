#include "ncurses.hh"

#include <ncurses.h>
#include <stdexcept>
#include "utf8.hh"

namespace curses {

// get an array of bytes represents a UTF-8 character from window.
std::string get_utf8_char(WINDOW* window)
{
  std::array<uint8_t, 6> buf{0};

  auto ch0 = static_cast<uint8_t>(::wgetch(window) & 0x000000FF);
  size_t len = get_utf8_char_length(ch0);
  buf[0] = ch0;

  for (size_t i = 1; i < len; ++i) {
    auto ch = static_cast<uint8_t>(::wgetch(window) & 0x000000FF);
    if (!is_utf8_cont(ch)) {
      throw std::runtime_error(std::string(__FUNCTION__) + ": wrong byte exists");
    }
    buf[i] = ch;
  }

  return std::string(buf.data(), buf.data() + len);
}

Window::Window()
{
  tty_in.reset(fopen("/dev/tty", "r"));
  tty_out.reset(fopen("/dev/tty", "w"));

  // create a new terminal
  scr = ::newterm(getenv("TERM"), tty_out.get(), tty_in.get());
  win = stdscr;

  // setup
  ::noecho();           // do not echo back characters
  ::cbreak();           // without buffering
  ::keypad(win, true);  // convert escape sequeces to key code
  ::wtimeout(win, 1);   // set delay time

  // initialize colormap.
  // start_color();
  // ::init_pair(1, COLOR_RED, COLOR_WHITE);
}

Window::~Window()
{
  // reset current screen.
  ::endwin();

  // release all resources of current session.
  ::delscreen(scr);
}

void Window::erase() { ::werase(win); }

void Window::refresh() { ::wrefresh(win); }

std::tuple<int, int> Window::get_size() const
{
  int width, height;
  getmaxyx(win, height, width);
  return std::make_tuple(width, height);
}

void Window::add_str(int x, int y, std::string const& text) { mvwaddstr(win, y, x, text.c_str()); }

void Window::change_attr(int x, int y, int n, int col) { mvwchgat(win, y, x, n, A_BOLD | A_UNDERLINE, col, nullptr); }

Event Window::poll_event()
{
  int ch = ::wgetch(win);
  if (ch == 10) {
    return Event{Key::Enter};
  }
  else if (ch == 27) {
    int ch = ::wgetch(win);
    if (ch == -1) {
      return Event{Key::Esc};
    }
    else {
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
  else if (ch == 127 || ch == KEY_BACKSPACE) {
    return Event{Key::Backspace};
  }
  else if (ch == 18) {
    return Event{Key::Ctrl, 'r'};
  }
  else if (is_utf8_first(ch & 0xFF)) {
    ::ungetch(ch);
    auto ch = get_utf8_char(win);
    return Event{std::move(ch)};
  }
  else {
    return Event{Key::Unknown};
  }
}

} // namespace curses;

