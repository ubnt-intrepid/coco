#include "utf8.hh"
#include <stdexcept>
#include <locale>
#include <codecvt>

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

std::size_t get_mb_width(std::string const& s)
{
  if (get_utf8_char_length(s[0]) < s.size()) {
    throw std::runtime_error(std::string(__FUNCTION__) + ": a UTF-8 character is only allowed.");
  }

  // http://php.net/manual/ja/function.mb-strwidth.php
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
  std::u32string u32str = converter.from_bytes(s);
  if (u32str[0] < 0x0020) {
    return 0;
  }
  else if (u32str[0] >= 0x0020 && u32str[0] < 0x2000) {
    return 1;
  }
  else if (u32str[0] >= 0x2000 && u32str[0] < 0xFF61) {
    return 2;
  }
  else if (u32str[0] >= 0xFF61 && u32str[0] < 0xFF9F) {
    return 1;
  }
  else {
    return 2;
  }
}