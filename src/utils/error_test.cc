#include <gtest/gtest.h>
#include <iostream>
#include "absl/strings/match.h"

#include "src/utils/codes/error_codes.pb.h"
#include "src/utils/error.h"
#include "src/utils/error_strings.h"

namespace pl {
namespace error {

TEST(CodeToString, strings) {
  EXPECT_EQ("OK", CodeToString(pl::error::OK));
  EXPECT_EQ("Cancelled", CodeToString(pl::error::CANCELLED));
  EXPECT_EQ("Unknown", CodeToString(pl::error::UNKNOWN));
  EXPECT_EQ("Invalid Argument", CodeToString(pl::error::INVALID_ARGUMENT));
  EXPECT_EQ("Deadline Exceeded", CodeToString(pl::error::DEADLINE_EXCEEDED));
  EXPECT_EQ("Not Found", CodeToString(pl::error::NOT_FOUND));
  EXPECT_EQ("Already Exists", CodeToString(pl::error::ALREADY_EXISTS));
  EXPECT_EQ("Permission Denied", CodeToString(pl::error::PERMISSION_DENIED));
  EXPECT_EQ("Unauthenticated", CodeToString(pl::error::UNAUTHENTICATED));
  EXPECT_EQ("Internal", CodeToString(pl::error::INTERNAL));
  EXPECT_EQ("Unimplemented", CodeToString(pl::error::UNIMPLEMENTED));
}

TEST(CodeToString, UNKNOWN_ERROR) {
  EXPECT_TRUE(
      absl::StartsWith(CodeToString(static_cast<pl::error::Code>(1024)), "Unknown error_code"));
}

TEST(ErrorDeclerations, Cancelled) {
  Status cancelled = Cancelled("test_message $0", 0);
  ASSERT_TRUE(IsCancelled(cancelled));
  ASSERT_EQ("test_message 0", cancelled.msg());
}

}  // namespace error
}  // namespace pl
