#include <gtest/gtest.h>

#include "utf8.hh"

TEST(utf8_test, is_utf8_first) {
  EXPECT_TRUE(is_utf8_first('a'));
  EXPECT_TRUE(is_utf8_first('0'));

  char hoge[] = "ほげ";
  EXPECT_TRUE(is_utf8_first(hoge[0]));

  EXPECT_FALSE(is_utf8_first(0xA2));
}
