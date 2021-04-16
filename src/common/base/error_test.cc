#include <absl/strings/match.h>
#include <gtest/gtest.h>
#include <iostream>

#include "src/common/base/error.h"
#include "src/common/base/statuspb/status.pb.h"

namespace px {
namespace error {

TEST(CodeToString, strings) {
  EXPECT_EQ("Ok", CodeToString(px::statuspb::OK));
  EXPECT_EQ("Cancelled", CodeToString(px::statuspb::CANCELLED));
  EXPECT_EQ("Unknown", CodeToString(px::statuspb::UNKNOWN));
  EXPECT_EQ("Invalid Argument", CodeToString(px::statuspb::INVALID_ARGUMENT));
  EXPECT_EQ("Deadline Exceeded", CodeToString(px::statuspb::DEADLINE_EXCEEDED));
  EXPECT_EQ("Not Found", CodeToString(px::statuspb::NOT_FOUND));
  EXPECT_EQ("Already Exists", CodeToString(px::statuspb::ALREADY_EXISTS));
  EXPECT_EQ("Permission Denied", CodeToString(px::statuspb::PERMISSION_DENIED));
  EXPECT_EQ("Unauthenticated", CodeToString(px::statuspb::UNAUTHENTICATED));
  EXPECT_EQ("Internal", CodeToString(px::statuspb::INTERNAL));
  EXPECT_EQ("Unimplemented", CodeToString(px::statuspb::UNIMPLEMENTED));
}

TEST(CodeToString, UNKNOWN_ERROR) {
  EXPECT_TRUE(
      absl::StartsWith(CodeToString(static_cast<px::statuspb::Code>(1024)), "Unknown error_code"));
}

TEST(ErrorDeclerations, Cancelled) {
  Status cancelled = Cancelled("test_message $0", 0);
  ASSERT_TRUE(IsCancelled(cancelled));
  ASSERT_EQ("test_message 0", cancelled.msg());
}

}  // namespace error
}  // namespace px
