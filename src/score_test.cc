#include <gtest/gtest.h>
#include "score.hh"

TEST(score_test, score_by_regex1)
{
  auto score = score_by_regex(R"(aaa)");

  EXPECT_EQ(1.0, score("aaa"));
  EXPECT_EQ(1.0, score("bb aaab"));

  EXPECT_EQ(0.0, score("bbaaba"));
}

TEST(score_test, score_by_regex2)
{
  auto score = score_by_regex(u8"ほげ");

  EXPECT_EQ(1.0, score(u8"ほげほげ"));
  EXPECT_EQ(0.0, score(u8"🍣🐟💰"));
}
