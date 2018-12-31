#include <gtest/gtest.h>

#include "src/utils/status.h"

namespace pl {

TEST(Status, Default) {
  Status status;
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(status, Status::OK());
  EXPECT_EQ(status.code(), pl::error::OK);
}

TEST(Status, EqCopy) {
  Status a(pl::error::UNKNOWN, "Badness");
  Status b = a;

  ASSERT_EQ(a, b);
}

TEST(Status, EqDiffCode) {
  Status a(pl::error::UNKNOWN, "Badness");
  Status b(pl::error::CANCELLED, "Badness");

  ASSERT_NE(a, b);
}

}  // namespace pl
