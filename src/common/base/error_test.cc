#include <absl/strings/match.h>
#include <gtest/gtest.h>
#include <iostream>

#include "src/common/base/error.h"
#include "src/common/base/statuspb/status.pb.h"

namespace pl {
namespace error {

TEST(CodeToString, strings) {
  EXPECT_EQ("Ok", CodeToString(pl::statuspb::OK));
  EXPECT_EQ("Cancelled", CodeToString(pl::statuspb::CANCELLED));
  EXPECT_EQ("Unknown", CodeToString(pl::statuspb::UNKNOWN));
  EXPECT_EQ("Invalid Argument", CodeToString(pl::statuspb::INVALID_ARGUMENT));
  EXPECT_EQ("Deadline Exceeded", CodeToString(pl::statuspb::DEADLINE_EXCEEDED));
  EXPECT_EQ("Not Found", CodeToString(pl::statuspb::NOT_FOUND));
  EXPECT_EQ("Already Exists", CodeToString(pl::statuspb::ALREADY_EXISTS));
  EXPECT_EQ("Permission Denied", CodeToString(pl::statuspb::PERMISSION_DENIED));
  EXPECT_EQ("Unauthenticated", CodeToString(pl::statuspb::UNAUTHENTICATED));
  EXPECT_EQ("Internal", CodeToString(pl::statuspb::INTERNAL));
  EXPECT_EQ("Unimplemented", CodeToString(pl::statuspb::UNIMPLEMENTED));
}

TEST(CodeToString, UNKNOWN_ERROR) {
  EXPECT_TRUE(
      absl::StartsWith(CodeToString(static_cast<pl::statuspb::Code>(1024)), "Unknown error_code"));
}

TEST(ErrorDeclerations, Cancelled) {
  Status cancelled = Cancelled("test_message $0", 0);
  ASSERT_TRUE(IsCancelled(cancelled));
  ASSERT_EQ("test_message 0", cancelled.msg());
}

}  // namespace error
}  // namespace pl
