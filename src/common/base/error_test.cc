/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
