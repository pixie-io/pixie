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

#include "src/common/base/status.h"

#include <google/protobuf/util/message_differencer.h>
#include <iostream>

#include <absl/strings/str_format.h>
#include "src/common/base/testproto/test.pb.h"
#include "src/common/testing/testing.h"

namespace px {

using ::px::testing::status::StatusIs;

TEST(Status, Default) {
  Status status;
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(status, Status::OK());
  EXPECT_EQ(status.code(), px::statuspb::OK);
}

TEST(Status, EqCopy) {
  Status a(px::statuspb::UNKNOWN, "Badness");
  Status b = a;

  ASSERT_EQ(a, b);
}

TEST(Status, EqDiffCode) {
  Status a(px::statuspb::UNKNOWN, "Badness");
  Status b(px::statuspb::CANCELLED, "Badness");

  ASSERT_NE(a, b);
}

Status MacroTestFn(const Status& s) {
  PX_RETURN_IF_ERROR(s);
  return Status::OK();
}

TEST(Status, pl_return_if_error_test) {
  EXPECT_EQ(Status::OK(), MacroTestFn(Status::OK()));

  auto err_status = Status(px::statuspb::UNKNOWN, "an error");
  EXPECT_EQ(err_status, MacroTestFn(err_status));

  // Check to make sure value to macro is used only once.
  int call_count = 0;
  auto fn = [&]() -> Status {
    call_count++;
    return Status::OK();
  };
  auto test_fn = [&]() -> Status {
    PX_RETURN_IF_ERROR(fn());
    return Status::OK();
  };
  EXPECT_OK(test_fn());
  EXPECT_EQ(1, call_count);
}

TEST(Status, to_proto) {
  Status s1(px::statuspb::UNKNOWN, "error 1");
  auto pb1 = s1.ToProto();
  EXPECT_EQ(px::statuspb::UNKNOWN, pb1.err_code());
  EXPECT_EQ("error 1", pb1.msg());

  Status s2(px::statuspb::INVALID_ARGUMENT, "error 2");
  auto pb2 = s2.ToProto();
  EXPECT_EQ(px::statuspb::INVALID_ARGUMENT, pb2.err_code());
  EXPECT_EQ("error 2", pb2.msg());

  px::statuspb::Status status_proto;
  s2.ToProto(&status_proto);
  EXPECT_EQ(s2, Status(status_proto));
}

TEST(Status, no_context_tests) {
  Status s1(px::statuspb::UNKNOWN, "error 1");
  EXPECT_FALSE(s1.has_context());
}

std::unique_ptr<google::protobuf::Message> MakeTestMessage() {
  auto parent_pb = std::make_unique<testpb::TestParentMessage>();
  parent_pb->set_int_val(801);
  testpb::TestChildMessage* child_pb = parent_pb->add_child();
  child_pb->set_string_val("test_value");

  return parent_pb;
}

TEST(Status, context_copy_tests) {
  Status s1(px::statuspb::UNKNOWN, "error 1", MakeTestMessage());
  EXPECT_TRUE(s1.has_context());
  Status s2 = s1;
  EXPECT_TRUE(s2.has_context());
  EXPECT_EQ(s1, s2);
  EXPECT_EQ(s1.context()->DebugString(), s2.context()->DebugString());
}

TEST(Status, context_vs_no_context_status) {
  Status s1(px::statuspb::UNKNOWN, "error 1", MakeTestMessage());
  Status s2(s1.code(), s1.msg());
  EXPECT_NE(s1, s2);
  EXPECT_FALSE(s2.has_context());
  EXPECT_EQ(s2.context(), nullptr);
  EXPECT_NE(s1.ToProto().DebugString(), s2.ToProto().DebugString());
}

TEST(StatusAdapter, proto_with_context_test) {
  // from_proto
  Status s1(px::statuspb::UNKNOWN, "error 1", MakeTestMessage());
  auto pb1 = s1.ToProto();
  auto s2 = StatusAdapter(pb1);
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(s1.ToProto(), s2.ToProto()));
  EXPECT_EQ(s1, s2);
  // Confirm that copying yet again doesn't cause weird
  // nesting that wasn't called before.
  auto s3 = StatusAdapter(s2.ToProto());
  EXPECT_EQ(s1, s3);
}

TEST(Status, context_nullptr_test) {
  // nullptr for context should mean statuses are the same.
  Status s1(px::statuspb::UNKNOWN, "error 1", nullptr);
  Status s2(px::statuspb::UNKNOWN, "error 1");
  EXPECT_EQ(s1, s2);
}

TEST(StatusAdapter, from_proto) {
  Status s1(px::statuspb::UNKNOWN, "error 1");
  auto pb1 = s1.ToProto();
  EXPECT_EQ(s1, StatusAdapter(pb1));
}

TEST(StatusAdapter, from_proto_without_error) {
  auto pb1 = Status::OK().ToProto();
  std::cout << pb1.DebugString() << std::endl;
  EXPECT_TRUE(Status::OK() == StatusAdapter(pb1));
}

TEST(PrependMessage, ResultsIsAsExpected) {
  Status s1(px::statuspb::INTERNAL, "Internal");
  EXPECT_THAT(s1, StatusIs(px::statuspb::INTERNAL, "Internal"));
}

}  // namespace px
