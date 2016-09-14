#include "utf8.hh"
#include <stdexcept>

bool is_utf8_first(uint8_t ch)
{
  return (ch & 0xC0) != 0x80 && (ch & 0xFE) != 0xFE && ((ch & 0x80) == 0 || (ch & 0xC0) == 0xC0);
}

bool is_utf8_cont(uint8_t ch) { return (ch & 0xC0) == 0x80; }

size_t get_utf8_char_length(uint8_t ch)
{
  if (!is_utf8_first(ch)) {
    throw std::runtime_error(std::string(__FUNCTION__) + ": the first byte is not UTF8");
  }

  for (int i = 0; i < 6; ++i) {
    if ((ch & (0x01 << (7 - i))) == 0) {
      return std::max(1, i);
    }
  }

  throw std::runtime_error(std::string(__FUNCTION__) + ": unreachable");
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