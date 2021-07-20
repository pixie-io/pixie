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

#include "src/stirling/source_connectors/socket_tracer/protocols/nats/stitcher.h"

#include <string>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {

using ::px::stirling::protocols::nats::Message;
using ::px::stirling::protocols::nats::Record;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;

auto EqualsRecord(std::string_view req_cmd, std::string_view resp_cmd) {
  return AllOf(Field(&Record::req, Field(&Message::command, req_cmd)),
               Field(&Record::resp, Field(&Message::command, resp_cmd)));
}

class NATSStitchTest : public ::testing::Test {
 protected:
  Message GenMsg(std::string_view command, std::string_view options) {
    Message msg;
    msg.timestamp_ns = ts_ns_++;
    msg.command = command;
    msg.options = options;
    return msg;
  }

  int64_t ts_ns_ = 0;
};

// Tests that messages without +OK or -ERR messages were exported as records.
TEST_F(NATSStitchTest, MessagesWithoutResponse) {
  std::deque<Message> reqs = {GenMsg("SUB", "sub options")};
  std::deque<Message> resps = {GenMsg("MSG", "content")};

  NoState no_state;

  RecordsWithErrorCount<Record> results = StitchFrames<Record>(&reqs, &resps, &no_state);
  EXPECT_EQ(results.error_count, 0);
  EXPECT_THAT(results.records, ElementsAre(EqualsRecord("SUB", ""), EqualsRecord("MSG", "")));
}

// Tests that messages matched with +OK and -ERR, and are exported as records.
TEST_F(NATSStitchTest, MessagesMatchesOKAndErr) {
  std::deque<Message> reqs = {GenMsg("SUB", "sub options")};
  std::deque<Message> resps = {GenMsg("-ERR", "content")};

  NoState no_state;

  RecordsWithErrorCount<Record> results = StitchFrames<Record>(&reqs, &resps, &no_state);
  EXPECT_EQ(results.error_count, 0);
  EXPECT_THAT(results.records, ElementsAre(EqualsRecord("SUB", "-ERR")));
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
