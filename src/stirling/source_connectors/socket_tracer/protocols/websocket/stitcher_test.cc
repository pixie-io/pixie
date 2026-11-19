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

#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/stitcher.h"

#include <deque>
#include <string>
#include <utility>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace websocket {

Frame MakeFrame(Opcode opcode, bool fin, std::string payload, uint64_t timestamp_ns,
                uint8_t rsv = 0) {
  Frame frame;
  frame.timestamp_ns = timestamp_ns;
  frame.fin = fin;
  frame.rsv = rsv;
  frame.opcode = opcode;
  frame.payload_length = payload.size();
  frame.payload = std::move(payload);
  return frame;
}

class WebSocketStitcherTest : public ::testing::Test {
 protected:
  RecordsWithErrorCount<Record> Stitch() {
    return StitchFrames<Record>(&req_frames_, &resp_frames_, &state_);
  }

  std::deque<Frame> req_frames_;
  std::deque<Frame> resp_frames_;
  NoState state_;
};

TEST_F(WebSocketStitcherTest, OneRecordPerMessageInEachDirection) {
  req_frames_.push_back(MakeFrame(Opcode::kText, true, "hi", 1));
  resp_frames_.push_back(MakeFrame(Opcode::kBinary, true, "yo", 2));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 2U);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records[0].msg.type, message_type_t::kRequest);
  EXPECT_EQ(result.records[0].msg.opcode, Opcode::kText);
  EXPECT_EQ(result.records[0].msg.payload, "hi");
  EXPECT_EQ(result.records[0].msg.timestamp_ns, 1U);
  EXPECT_EQ(result.records[0].msg.fragments, 1U);
  EXPECT_EQ(result.records[1].msg.type, message_type_t::kResponse);
  EXPECT_EQ(result.records[1].msg.opcode, Opcode::kBinary);
  EXPECT_EQ(result.records[1].msg.payload, "yo");
  EXPECT_TRUE(req_frames_.empty());
  EXPECT_TRUE(resp_frames_.empty());
}

TEST_F(WebSocketStitcherTest, FragmentsAreReassembled) {
  req_frames_.push_back(MakeFrame(Opcode::kText, false, "Hel", 1, 0x4));
  req_frames_.push_back(MakeFrame(Opcode::kContinuation, true, "lo", 3));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_EQ(result.records[0].msg.opcode, Opcode::kText);
  EXPECT_EQ(result.records[0].msg.payload, "Hello");
  EXPECT_EQ(result.records[0].msg.timestamp_ns, 1U);
  EXPECT_EQ(result.records[0].msg.fragments, 2U);
  EXPECT_TRUE(result.records[0].msg.compressed);
  EXPECT_TRUE(req_frames_.empty());
}

TEST_F(WebSocketStitcherTest, ControlFrameBetweenFragments) {
  req_frames_.push_back(MakeFrame(Opcode::kText, false, "Hel", 1));
  req_frames_.push_back(MakeFrame(Opcode::kPing, true, "p", 2));
  req_frames_.push_back(MakeFrame(Opcode::kContinuation, true, "lo", 3));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 2U);
  EXPECT_EQ(result.records[0].msg.opcode, Opcode::kPing);
  EXPECT_EQ(result.records[0].msg.payload, "p");
  EXPECT_EQ(result.records[1].msg.opcode, Opcode::kText);
  EXPECT_EQ(result.records[1].msg.payload, "Hello");
  EXPECT_TRUE(req_frames_.empty());
}

TEST_F(WebSocketStitcherTest, IncompleteMessageWaitsForItsLastFragment) {
  req_frames_.push_back(MakeFrame(Opcode::kText, false, "Hel", 1));
  req_frames_.push_back(MakeFrame(Opcode::kPing, true, "p", 2));

  RecordsWithErrorCount<Record> first = Stitch();

  ASSERT_EQ(first.records.size(), 1U);
  EXPECT_EQ(first.records[0].msg.opcode, Opcode::kPing);
  ASSERT_EQ(req_frames_.size(), 1U);
  EXPECT_EQ(req_frames_.front().payload, "Hel");

  req_frames_.push_back(MakeFrame(Opcode::kContinuation, true, "lo", 3));
  RecordsWithErrorCount<Record> second = Stitch();

  ASSERT_EQ(second.records.size(), 1U);
  EXPECT_EQ(second.records[0].msg.payload, "Hello");
  EXPECT_TRUE(req_frames_.empty());
}

TEST_F(WebSocketStitcherTest, ContinuationWithoutStartIsAnError) {
  req_frames_.push_back(MakeFrame(Opcode::kContinuation, true, "lo", 1));
  req_frames_.push_back(MakeFrame(Opcode::kText, true, "next", 2));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_EQ(result.records[0].msg.payload, "next");
  EXPECT_EQ(result.error_count, 1);
  EXPECT_TRUE(req_frames_.empty());
}

TEST_F(WebSocketStitcherTest, NewMessageBeforeLastFragmentIsAnError) {
  req_frames_.push_back(MakeFrame(Opcode::kText, false, "lost", 1));
  req_frames_.push_back(MakeFrame(Opcode::kBinary, true, "next", 2));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_EQ(result.records[0].msg.opcode, Opcode::kBinary);
  EXPECT_EQ(result.records[0].msg.payload, "next");
  EXPECT_EQ(result.error_count, 1);
  EXPECT_TRUE(req_frames_.empty());
}

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
