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

#include <absl/strings/str_format.h>
#include <absl/strings/substitute.h>
#include <google/protobuf/util/message_differencer.h>

#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/common/base/status.h"
#include "src/common/base/statuspb/status.pb.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace planner {

TEST(CompilerErrorContextStatus, Default) {
  int64_t line = 10;
  int64_t column = 12;
  int64_t num_errors = 4;
  std::string message = "There's an error here.";
  compilerpb::CompilerErrorGroup errorgroup_in, errorgroup_out;
  for (int64_t i = 0; i < num_errors; i++) {
    compilerpb::CompilerError* error_parent = errorgroup_in.add_errors();
    compilerpb::LineColError* error = error_parent->mutable_line_col_error();

    error->set_line(line + i);
    error->set_column(column + i);
    error->set_message(absl::Substitute("msg: $0, idx: $1", message, i));
  }
  Status status(px::statuspb::INVALID_ARGUMENT, "Issue",
                std::make_unique<compilerpb::CompilerErrorGroup>(errorgroup_in));

  px::statuspb::Status status_pb = status.ToProto();
  ASSERT_TRUE(status_pb.context().Is<compilerpb::CompilerErrorGroup>());

  status_pb.context().UnpackTo(&errorgroup_out);
  EXPECT_EQ(errorgroup_in.DebugString(), errorgroup_out.DebugString());
  for (int64_t i = 0; i < errorgroup_in.errors_size(); i++) {
    auto error_parent_out = errorgroup_in.errors(i);
    auto error_out = error_parent_out.line_col_error();
    EXPECT_EQ(error_out.line(), line + i);
    EXPECT_EQ(error_out.column(), column + i);
    EXPECT_EQ(error_out.message(), absl::Substitute("msg: $0, idx: $1", message, i));
  }
}

TEST(CompilerErrorBuilder, LineColErrorPb) {
  std::string error_msg1 = "Error ova here.";
  compilerpb::CompilerErrorGroup error1 = LineColErrorPb(1, 2, error_msg1);

  // Parallel construction to make sure child content is created properly.
  compilerpb::LineColError line_col_error;
  line_col_error.set_line(1);
  line_col_error.set_column(2);
  line_col_error.set_message(error_msg1);

  ASSERT_EQ(error1.errors_size(), 1);
  EXPECT_TRUE(error1.errors(0).has_line_col_error());
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(error1.errors(0).line_col_error(),
                                                                 line_col_error));
}

TEST(CompilerErrorBuilder, MergedGroups) {
  compilerpb::CompilerErrorGroup error1 = LineColErrorPb(1, 2, "Error ova here.");
  compilerpb::CompilerErrorGroup error2 = LineColErrorPb(20, 19, "Error ova there.");
  compilerpb::CompilerErrorGroup error3 = LineColErrorPb(20, 4, "Error right here.");
  std::vector<compilerpb::CompilerErrorGroup> all_errors = {error1, error2, error3};

  compilerpb::CompilerErrorGroup merged_errors = MergeGroups(all_errors);
  ASSERT_EQ(merged_errors.errors_size(), 3);
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      merged_errors.errors(0).line_col_error(), error1.errors(0).line_col_error()));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      merged_errors.errors(1).line_col_error(), error2.errors(0).line_col_error()));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      merged_errors.errors(2).line_col_error(), error3.errors(0).line_col_error()));
}

TEST(CompilerErrorBuilder, MergedStatuses) {
  compilerpb::CompilerErrorGroup error1 = LineColErrorPb(1, 2, "Error ova here.");
  compilerpb::CompilerErrorGroup error2 = LineColErrorPb(20, 19, "Error ova there.");
  compilerpb::CompilerErrorGroup error3 = LineColErrorPb(20, 4, "Error right here.");
  std::vector<compilerpb::CompilerErrorGroup> all_errors = {error1, error2, error3};

  compilerpb::CompilerErrorGroup merged_errors = MergeGroups(all_errors);

  Status s1(statuspb::INVALID_ARGUMENT, "ContextError",
            std::make_unique<compilerpb::CompilerErrorGroup>(error1));
  Status s2(s1.code(), s1.msg(), std::make_unique<compilerpb::CompilerErrorGroup>(error2));
  Status s3(s1.code(), s1.msg(), std::make_unique<compilerpb::CompilerErrorGroup>(error3));
  Status merged_statuses = MergeStatuses({s1, s2, s3});
  EXPECT_EQ(Status(s1.code(), absl::StrJoin({s1.msg(), s2.msg(), s3.msg()}, "\n"),
                   std::make_unique<compilerpb::CompilerErrorGroup>(merged_errors)),
            merged_statuses);
}

TEST(CompilerErrorBuilder, EmptyStatusesVector) {
  // should return ok
  EXPECT_OK(MergeStatuses({}));
}
}  // namespace planner
}  // namespace carnot
}  // namespace px
