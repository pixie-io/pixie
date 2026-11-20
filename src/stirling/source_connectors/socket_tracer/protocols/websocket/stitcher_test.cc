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
#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/parse.h"

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
  StateWrapper state_;
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
  req_frames_.push_back(MakeFrame(Opcode::kText, false, "Hel", 1));
  req_frames_.push_back(MakeFrame(Opcode::kContinuation, true, "lo", 3));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_EQ(result.records[0].msg.opcode, Opcode::kText);
  EXPECT_EQ(result.records[0].msg.payload, "Hello");
  EXPECT_EQ(result.records[0].msg.payload_size, 5U);
  EXPECT_EQ(result.records[0].msg.timestamp_ns, 1U);
  EXPECT_EQ(result.records[0].msg.fragments, 2U);
  EXPECT_FALSE(result.records[0].msg.compressed);
  EXPECT_TRUE(req_frames_.empty());
}

TEST_F(WebSocketStitcherTest, CompressedPayloadIsRemoved) {
  req_frames_.push_back(MakeFrame(Opcode::kText, false, "\xf2\x48", 1, 0x4));
  req_frames_.push_back(MakeFrame(Opcode::kContinuation, true, "\xcd", 3));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_TRUE(result.records[0].msg.compressed);
  EXPECT_EQ(result.records[0].msg.payload, "<removed: compressed>");
  EXPECT_EQ(result.records[0].msg.payload_size, 3U);
}

TEST_F(WebSocketStitcherTest, TruncatedFrameKeepsItsPayloadSize) {
  Frame frame = MakeFrame(Opcode::kBinary, true, "abc", 1);
  frame.payload_length = 1000;
  resp_frames_.push_back(std::move(frame));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_EQ(result.records[0].msg.payload, "abc");
  EXPECT_EQ(result.records[0].msg.payload_size, 1000U);
}

TEST_F(WebSocketStitcherTest, ReassembledPayloadIsCappedAtLimit) {
  PX_SET_FOR_SCOPE(FLAGS_websocket_payload_limit_bytes, 4);
  req_frames_.push_back(MakeFrame(Opcode::kText, false, "Hel", 1));
  req_frames_.push_back(MakeFrame(Opcode::kContinuation, false, "lo", 2));
  req_frames_.push_back(MakeFrame(Opcode::kContinuation, true, "!!", 3));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_EQ(result.records[0].msg.payload, "Hell");
  EXPECT_EQ(result.records[0].msg.payload_size, 7U);
  EXPECT_EQ(result.records[0].msg.fragments, 3U);
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

TEST_F(WebSocketStitcherTest, RawFramesCarrySubprotocol) {
  state_.global.subprotocol = "graphql-transport-ws";
  req_frames_.push_back(MakeFrame(Opcode::kText, true, R"({"type":"ping"})", 1));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_EQ(result.records[0].kind, RecordKind::kFrame);
  EXPECT_EQ(result.records[0].subprotocol, "graphql-transport-ws");
}

TEST_F(WebSocketStitcherTest, RawFramesCarryRequestPath) {
  state_.global.path = "/echo?room=1";
  req_frames_.push_back(MakeFrame(Opcode::kText, true, "hi", 1));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 1U);
  EXPECT_EQ(result.records[0].req_path, "/echo?room=1");
}

TEST_F(WebSocketStitcherTest, K8sExecSessionIsDecoded) {
  state_.global.subprotocol = "v5.channel.k8s.io";
  state_.global.path = "/api/v1/namespaces/ns/pods/pod/exec?command=hostname&stdout=true";
  resp_frames_.push_back(MakeFrame(Opcode::kBinary, true, "\x01pod\n", 1));
  resp_frames_.push_back(
      MakeFrame(Opcode::kBinary, true, "\x03{\"metadata\":{},\"status\":\"Success\"}", 2));
  resp_frames_.push_back(MakeFrame(Opcode::kClose, true, "\x03\xe8", 3));

  RecordsWithErrorCount<Record> result = Stitch();

  ASSERT_EQ(result.records.size(), 2U);
  EXPECT_EQ(result.records[0].kind, RecordKind::kExec);
  EXPECT_EQ(result.records[0].msg.payload, "hostname");
  EXPECT_EQ(result.records[1].kind, RecordKind::kExit);
  EXPECT_EQ(result.records[1].output, "pod\n");
  EXPECT_EQ(result.records[1].subprotocol, "v5.channel.k8s.io");
  EXPECT_EQ(result.records[1].req_path,
            "/api/v1/namespaces/ns/pods/pod/exec?command=hostname&stdout=true");
  EXPECT_TRUE(resp_frames_.empty());
}

TEST_F(WebSocketStitcherTest, K8sSessionClosedWithoutStatusIsFlushedOnce) {
  state_.global.subprotocol = "v5.channel.k8s.io";
  state_.global.path = "/api/v1/namespaces/ns/pods/pod/exec?command=hostname&stdout=true";
  state_.global.conn_closed = true;
  resp_frames_.push_back(MakeFrame(Opcode::kBinary, true, "\x01pod\n", 7));

  RecordsWithErrorCount<Record> first = Stitch();
  ASSERT_EQ(first.records.size(), 1U);
  EXPECT_EQ(first.records[0].kind, RecordKind::kExec);

  RecordsWithErrorCount<Record> second = Stitch();
  ASSERT_EQ(second.records.size(), 1U);
  EXPECT_EQ(second.records[0].kind, RecordKind::kExit);
  EXPECT_EQ(second.records[0].msg.payload, "");
  EXPECT_EQ(second.records[0].msg.timestamp_ns, 7U);
  EXPECT_EQ(second.records[0].output, "pod\n");
  EXPECT_EQ(second.records[0].subprotocol, "v5.channel.k8s.io");

  EXPECT_TRUE(Stitch().records.empty());
}

TEST_F(WebSocketStitcherTest, K8sSessionWithStatusIsNotFlushedAgain) {
  state_.global.subprotocol = "v5.channel.k8s.io";
  state_.global.path = "/api/v1/namespaces/ns/pods/pod/exec?command=hostname&stdout=true";
  state_.global.conn_closed = true;
  resp_frames_.push_back(MakeFrame(Opcode::kBinary, true, "\x01pod\n", 1));
  resp_frames_.push_back(
      MakeFrame(Opcode::kBinary, true, "\x03{\"metadata\":{},\"status\":\"Success\"}", 2));

  ASSERT_EQ(Stitch().records.size(), 2U);
  EXPECT_TRUE(Stitch().records.empty());
  EXPECT_TRUE(Stitch().records.empty());
}

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
