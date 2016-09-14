#ifndef __HEADER_UTF8__
#define __HEADER_UTF8__

#include <cstdio>
#include <string>

bool is_utf8_first(std::uint8_t ch);
bool is_utf8_cont(std::uint8_t ch);
std::size_t get_utf8_char_length(std::uint8_t ch);

void pop_back_utf8(std::string& str);

#endif