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

#include "src/stirling/source_connectors/socket_tracer/protocols/redis/stitcher.h"

#include <string>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::StrEq;

redis::Message CreateMsg(uint64_t ts_ns, std::string payload, std::string_view command) {
  redis::Message msg;
  msg.timestamp_ns = ts_ns;
  msg.payload = std::move(payload);
  msg.command = command;
  return msg;
}

redis::Message CreatePubMsg(uint64_t ts_ns, std::string payload, std::string_view command) {
  redis::Message msg = CreateMsg(ts_ns, payload, command);
  msg.is_published_message = true;
  return msg;
}

auto EqualsRecord(const std::string& req_cmd, const std::string& req_payload,
                  const std::string& resp_payload) {
  return AllOf(Field(&redis::Record::req,
                     // command field is std::string_view, we don't know how to cast Field() to
                     // std::string, and has to reply on the default == operator. The error message
                     // is very difficult to read.
                     AllOf(Field(&redis::Message::command, req_cmd),
                           Field(&redis::Message::payload, req_payload))),
               Field(&redis::Record::resp, Field(&redis::Message::payload, StrEq(resp_payload))));
}

// Tests that the published messages are exported correctly.
TEST(StitchFramesTest, PubMessagesExported) {
  std::deque<redis::Message> reqs;

  std::deque<redis::Message> resps;
  resps.push_back(CreatePubMsg(0, R"(["message", "foo", "test1"])", "MESSAGE"));
  resps.push_back(CreatePubMsg(1, R"(["message", "foo", "test2"])", "MESSAGE"));

  NoState no_state;

  RecordsWithErrorCount<redis::Record> res = StitchFrames<redis::Record>(&reqs, &resps, &no_state);
  EXPECT_EQ(res.error_count, 0);
  EXPECT_THAT(res.records,
              ElementsAre(EqualsRecord(R"(PUSH PUB)", "", R"(["message", "foo", "test1"])"),
                          EqualsRecord(R"(PUSH PUB)", "", R"(["message", "foo", "test2"])")));
  EXPECT_THAT(resps, IsEmpty());
}

// Tests that the command message in responses are exported directly.
TEST(StitchFramesTest, ReplConfAckAndMutationCommands) {
  std::deque<redis::Message> reqs = {
      CreateMsg(1, "repl_ack_payload_0", "REPLCONF ACK"),
      CreateMsg(3, "repl_ack_payload_1", "REPLCONF ACK"),
      CreateMsg(4, "", "PING"),
  };

  std::deque<redis::Message> resps = {
      CreateMsg(0, "hset_payload_0", "HSET"),
      CreateMsg(5, "PONG", ""),
  };

  NoState no_state;

  RecordsWithErrorCount<redis::Record> res = StitchFrames<redis::Record>(&reqs, &resps, &no_state);
  EXPECT_EQ(res.error_count, 0);
  EXPECT_THAT(res.records, ElementsAre(EqualsRecord("HSET", "hset_payload_0", ""),
                                       EqualsRecord("REPLCONF ACK", "repl_ack_payload_0", ""),
                                       EqualsRecord("REPLCONF ACK", "repl_ack_payload_1", ""),
                                       EqualsRecord("PING", "", "PONG")));
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
