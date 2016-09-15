#ifndef __HEADER_NCURSE__
#define __HEADER_NCURSE__

#include <string>
#include <cstdio>
#include <memory>

// forward declaration of curses structs.
struct screen;
struct _win_st;
typedef struct screen SCREEN;
typedef struct _win_st WINDOW;

namespace curses {

enum class Key { Enter, Esc, Ctrl, Alt, Up, Down, Left, Right, Tab, Backspace, Char, Unknown };

class Event {
  Key key;
  int mod;
  std::string ch;

public:
  Event(Key key) : key{key}, mod{0}, ch{} {}
  Event(Key key, int mod) : key{key}, mod{mod}, ch{} {}
  Event(std::string&& ch) : key{Key::Char}, mod{0}, ch{ch} {}

  std::string const& as_chars() const { return ch; }
  int get_mod() const { return mod; }

  inline bool operator==(Key key) const { return this->key == key; }
};

// wrapper of Ncurses API.
class Window {
  struct deleter_t {
    void operator()(FILE* fd) { ::fclose(fd); }
  };

  std::unique_ptr<FILE, deleter_t> tty_in, tty_out;
  SCREEN* scr = nullptr;
  WINDOW* win = nullptr;

public:
  Window();
  Window(Window&&) noexcept = default;
  ~Window();

  Event poll_event();

  void erase();
  void refresh();
  std::tuple<int, int> get_size() const;
  void add_str(int x, int y, std::string const& text);

  void change_attr(int x, int y, int n, int col);
};

} // namespace curses;

#endif
