#ifndef __HEADER_TERMUX_HPP__
#define __HEADER_TERMUX_HPP__

#include <termbox.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <stdexcept>

struct Event {
  tb_event ev;

public:
  bool is_key(uint32_t key) const { return ev.type == TB_EVENT_KEY && ev.key == key; }
  bool is_char() const { return ev.type == TB_EVENT_KEY && (ev.ch != 0 || ev.key == TB_KEY_SPACE); }
  uint32_t as_char() const
  {
    assert(is_char());
    return ev.key == TB_KEY_SPACE ? ' ' : ev.ch;
  }
};

class TermBox {
public:
  TermBox() { init(); }
  ~TermBox() noexcept
  {
    try {
      tb_shutdown();
    }
    catch (...) {
    }
  }
  void clear() { tb_clear(); }
  void flush() { tb_present(); }
  int width() const { return tb_width(); }
  int height() const { return tb_height(); }
  Event poll_event()
  {
    tb_event ev;
    tb_poll_event(&ev);
    return Event{std::move(ev)};
  }
  void putchar(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg) const { tb_change_cell(x, y, ch, fg, bg); }
  void println(int y, std::string const& line, std::uint16_t fg, std::uint16_t bg) const
  {
    for (int x = 0; x < tb_width(); ++x) {
      auto const ch = static_cast<uint32_t>(x < line.length() ? line[x] : ' ');
      tb_change_cell(x, y, ch, fg, bg);
    }
  }

private:
  void init()
  {
    int error = tb_init();
    if (error) {
      throw std::runtime_error{"tb_init() failed with error code " + std::to_string(error)};
    }
  }
};

#endif