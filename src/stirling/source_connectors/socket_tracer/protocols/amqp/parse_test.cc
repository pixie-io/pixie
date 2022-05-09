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

#include <string>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types.h"

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace amqp {

using ::testing::IsEmpty;
using ::testing::StrEq;

constexpr std::string_view kConnectionStart =
    "01000000000215000a000a0009000001f00c6361706162696c697469657346000000c7127075626c69736865725f63"
    "6f6e6669726d7374011a65786368616e67655f65786368616e67655f62696e64696e677374010a62617369632e6e61"
    "636b740116636f6e73756d65725f63616e63656c5f6e6f74696679740112636f6e6e656374696f6e2e626c6f636b65"
    "64740113636f6e73756d65725f7072696f72697469657374011c61757468656e7469636174696f6e5f6661696c7572"
    "655f636c6f73657401107065725f636f6e73756d65725f716f7374010f6469726563745f7265706c795f746f74010c"
    "636c75737465725f6e616d65530000002e7261626269744064656d6f2d5669766f426f6f6b2d415355532d4c617074"
    "6f702d583530355a412d583530355a4109636f707972696768745300000037436f7079726967687420286329203230"
    "30372d3230323220564d776172652c20496e632e206f722069747320616666696c69617465732e0b696e666f726d61"
    "74696f6e53000000394c6963656e73656420756e64657220746865204d504c20322e302e20576562736974653a2068"
    "747470733a2f2f7261626269746d712e636f6d08706c6174666f726d530000001145726c616e672f4f54502032342e"
    "332e340770726f6475637453000000085261626269744d510776657273696f6e5300000006332e31302e300000000e"
    "504c41494e20414d51504c41494e00000005656e5f5553ce";
constexpr std::string_view kConnectionStartOk =
    "01000000000140000a000b0000011c0770726f64756374530000000a7261626269746d712d630776657273696f6e53"
    "00000006302e31302e3008706c6174666f726d53000000054c696e757809636f707972696768745300000049436f70"
    "7972696768742028632920323030372d3230313420564d5761726520496e632c20546f6e79204761726e6f636b2d4a"
    "6f6e65732c20616e6420416c616e20416e746f6e756b2e0b696e666f726d6174696f6e530000002853656520687474"
    "70733a2f2f6769746875622e636f6d2f616c616e787a2f7261626269746d712d630c6361706162696c697469657346"
    "0000003c1c61757468656e7469636174696f6e5f6661696c7572655f636c6f736574011a65786368616e67655f6578"
    "6368616e67655f62696e64696e6773740105504c41494e0000000c00677565737400677565737405656e5f5553ce";
constexpr std::string_view kBasicPublish =
    "01000100000029003c002800000d736f6d655f65786368616e676513617765736f6d652d726f7574696e672d6b6579"
    "00ce";
constexpr std::string_view kMessageHeader = "0200010000000f003c0000000000000000000c100001ce";
constexpr std::string_view kMessageBody = "0300010000000c68656c6c6f2c20776f726c64ce";
constexpr std::string_view kHeartbeat = "08000000000000ce";

struct WellFormedTestCase {
  std::string_view input;
  Message::type expected_message_type;
  int16_t expected_message_channel;
  int32_t expected_message_length;
  std::string_view expected_message_body;
};

std::ostream& operator<<(std::ostream& os, const WellFormedTestCase& test_case) {
  os << "input: " << test_case.input
     << " type: " << magic_enum::enum_name(test_case.expected_message_type)
     << " channel: " << test_case.expected_message_channel
     << " length: " << test_case.expected_message_length
     << " body: " << test_case.expected_message_body;
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
        WellFormedTestCase{
            kConnectionStart, Message::type::kMethod, 0, 533,
            "000a000a0009000001f00c6361706162696c697469657346000000c7127075626c69736865725f636f6e66"
            "69726d7374011a65786368616e67655f65786368616e67655f62696e64696e677374010a62617369632e6e"
            "61636b740116636f6e73756d65725f63616e63656c5f6e6f74696679740112636f6e6e656374696f6e2e62"
            "6c6f636b6564740113636f6e73756d65725f7072696f72697469657374011c61757468656e746963617469"
            "6f6e5f6661696c7572655f636c6f73657401107065725f636f6e73756d65725f716f7374010f6469726563"
            "745f7265706c795f746f74010c636c75737465725f6e616d65530000002e7261626269744064656d6f2d56"
            "69766f426f6f6b2d415355532d4c6170746f702d583530355a412d583530355a4109636f70797269676874"
            "5300000037436f707972696768742028632920323030372d3230323220564d776172652c20496e632e206f"
            "722069747320616666696c69617465732e0b696e666f726d6174696f6e53000000394c6963656e73656420"
            "756e64657220746865204d504c20322e302e20576562736974653a2068747470733a2f2f7261626269746d"
            "712e636f6d08706c6174666f726d530000001145726c616e672f4f54502032342e332e340770726f647563"
            "7453000000085261626269744d510776657273696f6e5300000006332e31302e300000000e504c41494e20"
            "414d51504c41494e00000005656e5f5553"},

        WellFormedTestCase{
            kConnectionStartOk, Message::type::kMethod, 0, 320,
            "000a000b0000011c0770726f64756374530000000a7261626269746d712d630776657273696f6e53000000"
            "06302e31302e3008706c6174666f726d53000000054c696e757809636f707972696768745300000049436f"
            "707972696768742028632920323030372d3230313420564d5761726520496e632c20546f6e79204761726e"
            "6f636b2d4a6f6e65732c20616e6420416c616e20416e746f6e756b2e0b696e666f726d6174696f6e530000"
            "00285365652068747470733a2f2f6769746875622e636f6d2f616c616e787a2f7261626269746d712d630c"
            "6361706162696c6974696573460000003c1c61757468656e7469636174696f6e5f6661696c7572655f636c"
            "6f736574011a65786368616e67655f65786368616e67655f62696e64696e6773740105504c41494e000000"
            "0c00677565737400677565737405656e5f5553"},

        WellFormedTestCase{
            kBasicPublish, Message::type::kMethod, 1, 41,
            "003c002800000d736f6d655f65786368616e676513617765736f6d652d726f7574696e672d6b657900"},

        WellFormedTestCase{kMessageHeader, Message::type::kHeader, 1, 15,
                           "003c0000000000000000000c100001"},

        WellFormedTestCase{kMessageBody, Message::type::kBody, 1, 12, "68656c6c6f2c20776f726c64"},

        WellFormedTestCase{kHeartbeat, Message::type::kHeartbeat, 0, 0, ""}));

}  // namespace amqp
}  // namespace protocols
}  // namespace stirling
}  // namespace px
