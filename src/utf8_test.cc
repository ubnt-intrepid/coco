#include <gtest/gtest.h>

#include "utf8.hh"

TEST(utf8_test, is_utf8_first)
{
  EXPECT_TRUE(is_utf8_first('a'));
  EXPECT_TRUE(is_utf8_first('0'));

  std::string hoge = u8"ほ";
  EXPECT_TRUE(is_utf8_first(hoge[0]));

  EXPECT_FALSE(is_utf8_first(0xA2));
}

TEST(utf8_test, is_utf8_cont)
{
  std::string hoge = u8"ほ";
  EXPECT_FALSE(is_utf8_cont(hoge[0]));
  EXPECT_TRUE(is_utf8_cont(hoge[1]));
  EXPECT_TRUE(is_utf8_cont(hoge[2]));

  EXPECT_FALSE(is_utf8_cont('a'));
}

TEST(utf8_test, get_utf8_char_length)
{
  std::string hoge = u8"ほ";
  EXPECT_EQ(3, get_utf8_char_length(hoge[0]));

  hoge = u8"🍣";
  EXPECT_EQ(4, get_utf8_char_length(hoge[0]));
}

TEST(utf8_test, pop_back_utf8)
{
  std::string hoge = u8"ほげほaげ";

  pop_back_utf8(hoge);
  EXPECT_EQ(hoge, u8"ほげほa");

  pop_back_utf8(hoge);
  EXPECT_EQ(hoge, u8"ほげほ");

  hoge += u8"🍣";
  pop_back_utf8(hoge);
  EXPECT_EQ(hoge, u8"ほげほ");
}

TEST(utf8_test, get_mb_width) {
  EXPECT_EQ(1, get_mb_width(u8"a"));
  EXPECT_EQ(2, get_mb_width(u8"あ"));
  EXPECT_EQ(2, get_mb_width(u8"🍣"));
  EXPECT_EQ(0, get_mb_width(u8"\n"));
}
