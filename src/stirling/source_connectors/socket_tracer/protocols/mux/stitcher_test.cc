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

#include "src/stirling/source_connectors/socket_tracer/protocols/mux/stitcher.h"

#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/parse.h"

namespace px {
namespace stirling {
namespace protocols {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

class StitchFramesTest : public ::testing::Test {};

mux::Frame CreateMuxFrame(uint64_t ts_ns, mux::Type type, uint24_t tag) {
  mux::Frame frame;
  frame.timestamp_ns = ts_ns;
  frame.type = static_cast<int8_t>(type);
  frame.tag = tag;
  return frame;
}

TEST_F(StitchFramesTest, VerifyTransmitReceivePairsAreMatched) {
  std::deque<mux::Frame> reqs = {
      // tinit check message
      CreateMuxFrame(0, mux::Type::kRerrOld, 1),
      CreateMuxFrame(2, mux::Type::kTinit, 1),
      CreateMuxFrame(4, mux::Type::kTping, 1),
      CreateMuxFrame(6, mux::Type::kTdispatch, 1),
      CreateMuxFrame(8, mux::Type::kTdiscardedOld, 1),
      CreateMuxFrame(10, mux::Type::kTdiscarded, 1),
      CreateMuxFrame(12, mux::Type::kTreq, 1),
      CreateMuxFrame(14, mux::Type::kTdrain, 1),
  };
  std::deque<mux::Frame> resps = {
      // tinit check message response
      CreateMuxFrame(1, mux::Type::kRerrOld, 1),    CreateMuxFrame(3, mux::Type::kRinit, 1),
      CreateMuxFrame(5, mux::Type::kRping, 1),      CreateMuxFrame(7, mux::Type::kRdispatch, 1),
      CreateMuxFrame(9, mux::Type::kRdiscarded, 1), CreateMuxFrame(11, mux::Type::kRdiscarded, 1),
      CreateMuxFrame(13, mux::Type::kRreq, 1),      CreateMuxFrame(15, mux::Type::kRdrain, 1),
  };

  RecordsWithErrorCount<mux::Record> result = mux::StitchFrames(&reqs, &resps);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 8);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
}

TEST_F(StitchFramesTest, VerifyTleaseIsHandled) {
  std::deque<mux::Frame> reqs = {
      // tinit check message
      CreateMuxFrame(0, mux::Type::kRerrOld, 1),
      CreateMuxFrame(2, mux::Type::kTlease, 1),
      CreateMuxFrame(4, mux::Type::kTping, 1),
  };
  std::deque<mux::Frame> resps = {
      // tinit check message response
      CreateMuxFrame(1, mux::Type::kRerrOld, 1),
      CreateMuxFrame(5, mux::Type::kRping, 1),
  };

  RecordsWithErrorCount<mux::Record> result = mux::StitchFrames(&reqs, &resps);

  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 3);
  EXPECT_EQ(mux::Type(result.records[1].req.type), mux::Type::kTlease);
  // There is no response for Tlease so the response frame should
  // have an uninitialized type field
  EXPECT_EQ(result.records[1].resp.type, 0);

  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
}

TEST_F(StitchFramesTest, UnmatchedResponsesAreHandled) {
  std::deque<mux::Frame> reqs = {
      // tinit check message
      CreateMuxFrame(1, mux::Type::kRerrOld, 1),
  };
  std::deque<mux::Frame> resps = {
      // tinit check message response
      CreateMuxFrame(0, mux::Type::kRerrOld, 1),
      CreateMuxFrame(2, mux::Type::kRerrOld, 1),
  };

  RecordsWithErrorCount<mux::Record> result = mux::StitchFrames(&reqs, &resps);

  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 1);
  EXPECT_EQ(mux::Type(result.records[0].resp.type), mux::Type::kRerrOld);

  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
}

TEST_F(StitchFramesTest, UnmatchedRequestsAreNotCleanedUp) {
  std::deque<mux::Frame> reqs = {
      // tinit check message
      CreateMuxFrame(0, mux::Type::kRerrOld, 1),
      CreateMuxFrame(1, mux::Type::kTdispatch, 1),
      CreateMuxFrame(3, mux::Type::kTdrain, 1),
  };
  std::deque<mux::Frame> resps = {
      CreateMuxFrame(2, mux::Type::kRdispatch, 1),
      CreateMuxFrame(4, mux::Type::kRdrain, 1),
  };

  RecordsWithErrorCount<mux::Record> result = mux::StitchFrames(&reqs, &resps);

  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
  EXPECT_EQ(mux::Type(result.records[0].req.type), mux::Type::kTdispatch);
  EXPECT_EQ(mux::Type(result.records[1].req.type), mux::Type::kTdrain);

  // Since we can't know if the response pair has arrived
  // yet the stitcher must keep unmatched requests arround
  EXPECT_THAT(reqs.size(), 3);
  EXPECT_THAT(reqs[0].consumed, false);
  EXPECT_THAT(reqs[1].consumed, true);
  EXPECT_THAT(reqs[2].consumed, true);

  EXPECT_THAT(resps, IsEmpty());
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
