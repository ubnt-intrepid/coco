#include <ncurses.h>
#include <locale>
#include <chrono>
#include <thread>
#include <string>

bool is_utf8_start(uint8_t ch) {
  return (ch & 0xC0) == 0xC0;
}

bool is_utf8_cont(uint8_t ch) {
  return (ch & 0xC0) == 0x80;
}

int main()
{
  std::setlocale(LC_ALL, "");

  // initialize screen and create a new window. 
  WINDOW* win = ::initscr();

  ::noecho();
  ::cbreak();
  ::keypad(win, true);
  ::ESCDELAY = 25;

  int x = 0, y = 0;
  while (true)
  {
    int width, height;
    getmaxyx(win, height, width);

    ::erase();
    
    ::move(y, x);
    ::insstr("あ");
    
    ::refresh();

    int ch = ::getch();
    if (ch == KEY_UP || ch == 'k') {
      y = std::max(0, y - 1);
    }
    else if (ch == KEY_DOWN || ch == 'j') {
      y = std::min(height - 1, y + 1);
    }
    else if (ch == KEY_LEFT || ch == 'h') {
      x = std::max(0, x - 2);
    }
    else if (ch == KEY_RIGHT || ch == 'l') {
      x = std::min(width - 1, x + 2);
    }
    else if (ch == 10) { // 10 = enter
      y = std::min(height - 1, y + 1);
    }
    else if (ch == 127) {  // 127 = backspace
      x = std::max(0, x - 2);
    }
    else if (ch == 'q' || ch == 27) { // 27 = Esc or Alt+*
      ::nodelay(win, true);
      int ch = ::getch();
      if (ch == -1) {
        break;
      }
      ::ungetch(ch);
      ::nodelay(win, false);
    }
  }

  ::endwin();
}
