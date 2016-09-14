#include <gtest/gtest.h>
#include "score.hh"

static constexpr std::size_t INF = std::numeric_limits<std::size_t>::max();

TEST(score_test, score_by_regex1)
{
  auto score = score_by_regex(R"(aaa)");

  EXPECT_EQ(0, score("aaa"));
  EXPECT_EQ(0, score("bb aaab"));

  EXPECT_EQ(INF, score("bbaaba"));
}

TEST(score_test, score_by_regex2)
{
  auto score = score_by_regex(u8"ほげ");

  EXPECT_EQ(0, score(u8"ほげほげ"));
  EXPECT_EQ(INF, score(u8"🍣🐟💰"));
}
