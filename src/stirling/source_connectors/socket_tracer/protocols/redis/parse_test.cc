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

#include "src/stirling/source_connectors/socket_tracer/protocols/redis/parse.h"

#include <string>
#include <vector>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace redis {

using ::testing::IsEmpty;
using ::testing::StrEq;

constexpr std::string_view kSimpleStringMsg = "+OK\r\n";
constexpr std::string_view kErrorMsg = "-Error message\r\n";
constexpr std::string_view kBulkStringMsg = "$11\r\nbulk string\r\n";
constexpr std::string_view kEmptyBulkStringMsg = "$0\r\n\r\n";
constexpr std::string_view kNullBulkStringMsg = "$-1\r\n";
constexpr std::string_view kArrayMsg = "*3\r\n+OK\r\n-Error message\r\n$11\r\nbulk string\r\n";
constexpr std::string_view kNullElemInArrayMsg = "*1\r\n$-1\r\n";
constexpr std::string_view kNullArrayMsg = "*-1\r\n";
constexpr std::string_view kEmptyArrayMsg = "*0\r\n";
constexpr std::string_view kCmdMsg = "*2\r\n+ACL\r\n+LOAD\r\n";
constexpr std::string_view kPubMsg = "*3\r\n$7\r\nmessage\r\n$3\r\nfoo\r\n$4\r\ntest\r\n";
constexpr std::string_view kAppendMsg = "*3\r\n$6\r\nappend\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
constexpr std::string_view kAclGetuserMsg = "*2\r\n$11\r\nacl getuser\r\n$4\r\nuser\r\n";
constexpr std::string_view kAclDeluserMsg =
    "*3\r\n$11\r\nacl deluser\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
constexpr std::string_view kBrpopLPushMsg =
    "*4\r\n$10\r\nbrpoplpush\r\n$3\r\nsrc\r\n$4\r\ndest\r\n:10\r\n";
constexpr std::string_view kLPushMsg =
    "*4\r\n$5\r\nlpush\r\n$3\r\nfoo\r\n$4\r\nbar0\r\n$4\r\nbar1\r\n";
constexpr std::string_view kZPopMaxMsg = "*3\r\n$7\r\nzpopmax\r\n$3\r\nfoo\r\n:10\r\n";
constexpr std::string_view kZPopMaxNoOptArgMsg = "*2\r\n$7\r\nzpopmax\r\n$3\r\nfoo\r\n";
constexpr std::string_view kSwapDBMsg = "*3\r\n$6\r\nswapdb\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
constexpr std::string_view kEvalSHAMsg =
    "*5\r\n$7\r\nevalsha\r\n$3\r\nsha\r\n:3\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
constexpr std::string_view kHSetMsg = "*4\r\n$4\r\nhset\r\n$3\r\nfoo\r\n$3\r\nval\r\n:3\r\n";

struct WellFormedTestCase {
  std::string_view input;
  std::string_view expected_command;
  std::string_view expected_payload;
  std::vector<message_type_t> types_to_test = {message_type_t::kRequest, message_type_t::kResponse};
};

std::ostream& operator<<(std::ostream& os, const WellFormedTestCase& test_case) {
  os << "input: " << test_case.input << " payload: " << test_case.expected_payload
     << " command: " << test_case.expected_command;
  return os;
}

std::ostream& operator<<(std::ostream& os, ParseState state) {
  os << magic_enum::enum_name(state);
  return os;
}

class ParseTest : public ::testing::TestWithParam<WellFormedTestCase> {};

TEST_P(ParseTest, ResultsAreAsExpected) {
  for (message_type_t type : GetParam().types_to_test) {
    std::string_view req = GetParam().input;
    Message msg;

    EXPECT_EQ(ParseFrame(type, &req, &msg), ParseState::kSuccess);
    EXPECT_THAT(req, IsEmpty());
    EXPECT_THAT(msg.payload, StrEq(std::string(GetParam().expected_payload)));
    EXPECT_THAT(std::string(msg.command), StrEq(std::string(GetParam().expected_command)));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllDataTypes, ParseTest,
    ::testing::Values(
        // clang-format off
        WellFormedTestCase{kSimpleStringMsg, "", "OK"},
        WellFormedTestCase{kErrorMsg, "", "-Error message"},
        WellFormedTestCase{kBulkStringMsg, "", "bulk string"},
        WellFormedTestCase{kEmptyBulkStringMsg, "", ""},
        WellFormedTestCase{kNullBulkStringMsg, "", "<NULL>"},
        WellFormedTestCase{kArrayMsg, "", R"(["OK","-Error message","bulk string"])"},
        WellFormedTestCase{kCmdMsg, "ACL LOAD", R"([])", {message_type_t::kRequest}},
        WellFormedTestCase{kNullElemInArrayMsg, "", R"(["<NULL>"])"},
        WellFormedTestCase{kNullArrayMsg, "", "[NULL]"},
        WellFormedTestCase{kEmptyArrayMsg, "", "[]"},
        WellFormedTestCase{kAppendMsg, "APPEND", R"({"key":"foo","value":"bar"})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kAclGetuserMsg,  "ACL GETUSER", R"({"username":"user"})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kAclDeluserMsg,  "ACL DELUSER", R"({"username":["foo","bar"]})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kBrpopLPushMsg, "BRPOPLPUSH",
                           R"({"source":"src","destination":"dest","timeout":"10"})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kLPushMsg, "LPUSH", R"({"key":"foo","element":["bar0","bar1"]})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kZPopMaxMsg, "ZPOPMAX", R"({"key":"foo","count":"10"})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kZPopMaxNoOptArgMsg,  "ZPOPMAX", R"({"key":"foo"})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kSwapDBMsg, "SWAPDB", R"({"index1":"foo","index2":"bar"})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kEvalSHAMsg, "EVALSHA",
                           R"({"sha1":"sha","numkeys":"3","key":["foo"],"value":["bar"]})",
                           {message_type_t::kRequest}},
        WellFormedTestCase{kHSetMsg, "HSET",
                           R"({"key":"foo","field value":[{"field":"val"},{"value":"3"}]})",
                           {message_type_t::kRequest, message_type_t::kResponse}}));
// clang-format on

TEST(ParsePubMsgTest, DetectPublishedMessage) {
  std::string_view resp = kPubMsg;
  Message msg;

  EXPECT_EQ(ParseFrame(message_type_t::kResponse, &resp, &msg), ParseState::kSuccess);
  EXPECT_THAT(resp, IsEmpty());
  EXPECT_THAT(msg.payload, StrEq(R"(["message","foo","test"])"));
  EXPECT_TRUE(msg.is_published_message);
}

class ParseIncompleteInputTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ParseIncompleteInputTest, IncompleteInput) {
  std::string original_input = GetParam();
  std::string_view input = GetParam();
  Message msg;

  EXPECT_EQ(ParseFrame(message_type_t::kRequest, &input, &msg), ParseState::kNeedsMoreData);
  EXPECT_THAT(std::string(input), StrEq(original_input));
}

INSTANTIATE_TEST_SUITE_P(
    AllDataTypes, ParseIncompleteInputTest,
    ::testing::Values("+OK\r", "+OK", "+", "-Error message\r", "-Error message", "-",
                      "$11\r\nbulk string\r", "$11\r\nbulk string", "$11\r\nbulk", "$11\r\n",
                      "$11\r", "$11", "$", "*3\r\n+OK\r\n-Error message\r\n$11\r\nbulk string\r",
                      "*3\r\n+OK\r\n-Error message\r\n$11\r\nbulk string",
                      "*3\r\n+OK\r\n-Error message\r\n$11\r\nbulk ",
                      "*3\r\n+OK\r\n-Error message\r\n$11\r\n",
                      "*3\r\n+OK\r\n-Error message\r\n$11\r", "*3\r\n+OK\r\n-Error message\r\n$11",
                      "*3\r\n+OK\r\n-Error message\r\n", "*3\r\n+OK\r\n-Error message\r",
                      "*3\r\n+OK\r\n-Error message", "*3\r\n+OK\r\n", "*3\r\n+OK\r", "*3\r\n+OK",
                      "*3\r\n", "*3\r", "*3"));

class ParseInvalidInputTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ParseInvalidInputTest, InvalidInput) {
  std::string original_input = GetParam();
  std::string_view input = GetParam();
  Message msg;

  EXPECT_EQ(ParseFrame(message_type_t::kRequest, &input, &msg), ParseState::kInvalid);
  EXPECT_THAT(std::string(input), StrEq(original_input));
}

INSTANTIATE_TEST_SUITE_P(AllDataTypes, ParseInvalidInputTest,
                         ::testing::Values(
                             // Invalid markers
                             "a", "b", "c",
                             // Bulk string should end with \r\n
                             "$1\r\nabc"
                             // Length cannot be <-1
                             "$-2\r\n"
                             // Length cannot be <-1
                             "*-2\r\n"));

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace px
