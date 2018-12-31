#include <gtest/gtest.h>

#include "src/utils/statusor.h"

namespace pl {

using std::string;

TEST(StatusOr, ValueCopy) {
  string val = "abcd";
  StatusOr<string> s(val);
  ASSERT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), "abcd");
  EXPECT_EQ(val, "abcd");
}

TEST(StatusOr, ValueMove) {
  string val = "abcd";
  StatusOr<string> s(std::move(val));
  ASSERT_TRUE(s.ok());
  EXPECT_EQ(s.ConsumeValueOrDie(), "abcd");
  EXPECT_NE(val, "abcd");
}

TEST(StatusOr, ValuesAndErrors) {
  StatusOr<string> s("testing string");
  ASSERT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), "testing string");

  s = StatusOr<string>("another value");
  ASSERT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), "another value");

  s = StatusOr<string>(Status(pl::error::UNKNOWN, "some error"));
  ASSERT_FALSE(s.ok());
  EXPECT_EQ(s.msg(), "some error");
}

TEST(StatusOr, DefaultCtor) {
  StatusOr<string> s;
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), pl::error::UNKNOWN);
}

TEST(StatusOr, DefaultCtorValue) {
  StatusOr<string> s;
  EXPECT_DEATH(s.ValueOrDie(), "");
  EXPECT_DEATH(s.ConsumeValueOrDie(), "");
}

}  // namespace pl
