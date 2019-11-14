#include "src/common/base/statusor.h"
#include "src/common/testing/testing.h"

namespace pl {

using std::string;

TEST(StatusOr, ValueCopy) {
  string val = "abcd";
  StatusOr<string> s(val);
  ASSERT_OK(s);
  EXPECT_EQ(s.ValueOrDie(), "abcd");
  EXPECT_EQ(val, "abcd");
}

TEST(StatusOr, ValueMove) {
  string val = "abcd";
  StatusOr<string> s(std::move(val));
  ASSERT_OK(s);
  EXPECT_EQ(s.ConsumeValueOrDie(), "abcd");
  EXPECT_NE(val, "abcd");
}

TEST(StatusOr, ValuesAndErrors) {
  StatusOr<string> s("testing string");
  ASSERT_OK(s);
  EXPECT_EQ(s.ValueOrDie(), "testing string");

  s = StatusOr<string>("another value");
  ASSERT_OK(s);
  EXPECT_EQ(s.ValueOrDie(), "another value");

  s = StatusOr<string>(Status(pl::statuspb::UNKNOWN, "some error"));
  ASSERT_FALSE(s.ok());
  EXPECT_EQ(s.msg(), "some error");
}

TEST(StatusOr, DefaultCtor) {
  StatusOr<string> s;
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), pl::statuspb::UNKNOWN);
}

TEST(StatusOr, DefaultCtorValue) {
  StatusOr<string> s;
  EXPECT_DEATH(s.ValueOrDie(), "");
  EXPECT_DEATH(s.ConsumeValueOrDie(), "");
}

StatusOr<int> StatusOrTestFunc(int x) {
  if (x == 0) {
    return Status(pl::statuspb::INTERNAL, "badness");
  }
  return x + 1;
}

Status TestCheckCall(int x) {
  PL_ASSIGN_OR_RETURN(auto y, StatusOrTestFunc(x));
  EXPECT_EQ(y, x + 1);
  return Status::OK();
}

TEST(StatusOr, Macros) {
  EXPECT_TRUE(TestCheckCall(3).ok());
  Status s = TestCheckCall(0);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(pl::statuspb::INTERNAL, s.code());
}

}  // namespace pl
