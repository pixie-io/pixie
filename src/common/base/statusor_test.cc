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

#include <memory>

#include "src/common/base/statusor.h"
#include "src/common/testing/testing.h"

namespace px {

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

TEST(StatusOr, ValueOr) {
  StatusOr<string> s(Status(px::statuspb::UNKNOWN, "This is not OK"));
  EXPECT_EQ(s.ValueOr("pixie"), "pixie");
}

TEST(StatusOr, ConsumeValueOr) {
  {
    StatusOr<string> s(Status(px::statuspb::UNKNOWN, "This is not OK"));
    EXPECT_EQ(s.ConsumeValueOr("pixie"), "pixie");
  }

  {
    StatusOr<std::unique_ptr<int>> s(Status(px::statuspb::UNKNOWN, "This is not OK"));
    std::unique_ptr<int> val = s.ConsumeValueOr(std::make_unique<int>(2));
    EXPECT_EQ(*val, 2);
  }
}

TEST(StatusOr, ValuesAndErrors) {
  StatusOr<string> s("testing string");
  ASSERT_OK(s);
  EXPECT_EQ(s.ValueOrDie(), "testing string");

  s = StatusOr<string>("another value");
  ASSERT_OK(s);
  EXPECT_EQ(s.ValueOrDie(), "another value");

  s = StatusOr<string>(Status(px::statuspb::UNKNOWN, "some error"));
  ASSERT_FALSE(s.ok());
  EXPECT_EQ(s.msg(), "some error");
}

TEST(StatusOr, Pointers) {
  auto string_uptr = std::make_unique<std::string>();
  std::string* string_ptr = string_uptr.get();

  StatusOr<std::string*> s(string_ptr);
  ASSERT_OK(s);
  EXPECT_EQ(s.ValueOrDie(), string_ptr);
  EXPECT_EQ(s.ConsumeValueOrDie(), string_ptr);
}

TEST(StatusOr, NullPointer) {
  StatusOr<string*> s(nullptr);
  ASSERT_OK(s);
  EXPECT_EQ(s.ValueOrDie(), nullptr);
}

TEST(StatusOr, DefaultCtor) {
  StatusOr<string> s;
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), px::statuspb::UNKNOWN);
}

TEST(StatusOr, DefaultCtorValue) {
  StatusOr<string> s;
  EXPECT_DEATH(s.ValueOrDie(), "");
  EXPECT_DEATH(s.ConsumeValueOrDie(), "");
}

StatusOr<int> StatusOrTestFunc(int x) {
  if (x == 0) {
    return Status(px::statuspb::INTERNAL, "badness");
  }
  return x + 1;
}

Status TestCheckCall(int x) {
  PX_ASSIGN_OR_RETURN(auto y, StatusOrTestFunc(x));
  EXPECT_EQ(y, x + 1);
  return Status::OK();
}

TEST(StatusOr, Macros) {
  EXPECT_TRUE(TestCheckCall(3).ok());
  Status s = TestCheckCall(0);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(px::statuspb::INTERNAL, s.code());
}

}  // namespace px
