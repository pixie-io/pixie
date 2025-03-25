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

#include "src/stirling/source_connectors/socket_tracer/protocols/nats/parse.h"

#include <string>
#include <string_view>
#include <vector>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {

using ::testing::IsEmpty;
using ::testing::StrEq;

// Tests that the simple cases are as expected.
TEST(FindMessageBoundaryTest, ResultsAreAsExpected) {
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " CONNECT {} \r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " INFO {} \r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " PUB \r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " SUB \r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " UNSUB \r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " MSG \r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " PING\r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " PONG\r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " +OK\r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " -ERR 'test'\r\n", 0), 1);
  EXPECT_EQ(FindFrameBoundary<nats::Message>(message_type_t::kUnknown, " {} \r\n", 0),
            std::string_view::npos);
}

struct TestParam {
  std::string_view input;
  std::string command;
  std::string options;
};

using ParseFrameTest = ::testing::TestWithParam<TestParam>;

// Tests that parsing succeeded and the result is as expected.
TEST_P(ParseFrameTest, CheckResult) {
  TestParam param = GetParam();
  nats::Message msg;

  EXPECT_EQ(ParseFrame(message_type_t::kUnknown, &param.input, &msg), ParseState::kSuccess);
  EXPECT_THAT(msg.command, StrEq(param.command));
  EXPECT_THAT(msg.options, StrEq(param.options));
  EXPECT_THAT(param.input, IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    AllTypes, ParseFrameTest,
    ::testing::Values(
        TestParam{R"(INFO {"server_name":"localhost"} )"
                  "\r\n",
                  "INFO", R"({"server_name":"localhost"})"},
        TestParam{R"(CONNECT {"server_name":"localhost"} )"
                  "\r\n",
                  "CONNECT", R"({"server_name":"localhost"})"},
        TestParam{"SUB foo queue-group-1 1\r\n", "SUB",
                  R"({"queue group":"queue-group-1","sid":"1","subject":"foo"})"},
        TestParam{"SUB foo 1\r\n", "SUB", R"({"sid":"1","subject":"foo"})"},
        TestParam{"UNSUB 1 4\r\n", "UNSUB", R"({"max_msgs":"4","sid":"1"})"},
        TestParam{"UNSUB 1 \r\n", "UNSUB", R"({"sid":"1"})"},
        TestParam{"PUB subject-foo reply-to-bar 4\r\ntest\r\n", "PUB",
                  R"({"payload":"test","reply-to":"reply-to-bar","subject":"subject-foo"})"},
        TestParam{"PUB subject-foo 4\r\ntest\r\n", "PUB",
                  R"({"payload":"test","subject":"subject-foo"})"},
        TestParam{
            "MSG subject-foo 4 reply-to-bar 4\r\ntest\r\n", "MSG",
            R"({"payload":"test","reply-to":"reply-to-bar","sid":"4","subject":"subject-foo"})"},
        TestParam{"MSG subject-foo 4 4\r\ntest\r\n", "MSG",
                  R"({"payload":"test","sid":"4","subject":"subject-foo"})"},
        TestParam{"PING\r\n", "PING", ""}, TestParam{"PONG\r\n", "PONG", ""},
        TestParam{"+OK\r\n", "+OK", ""},
        TestParam{"-ERR 'error message'\r\n", "-ERR", R"('error message')"}));

}  // namespace protocols
}  // namespace stirling
}  // namespace px
