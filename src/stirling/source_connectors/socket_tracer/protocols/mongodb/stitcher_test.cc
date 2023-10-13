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

#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/stitcher.h"

#include <absl/container/flat_hash_map.h>

#include <string>
#include <utility>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mongodb {

using ::testing::IsEmpty;
using ::testing::SizeIs;

class MongoDBStitchFramesTest : public ::testing::Test {};

Frame CreateMongoDBFrame(uint64_t ts_ns, int32_t request_id, int32_t response_to, bool more_to_come,
                         std::string doc = "") {
  mongodb::Frame frame;
  frame.timestamp_ns = ts_ns;
  frame.request_id = request_id;
  frame.response_to = response_to;
  frame.more_to_come = more_to_come;

  mongodb::Section section;
  section.documents.push_back(doc);

  frame.sections.push_back(section);
  return frame;
}

bool areAllDequesEmpty(
    const absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>>& frame_map) {
  for (const auto& pair : frame_map) {
    if (!pair.second.empty()) {
      return false;
    }
  }
  return true;
}

size_t totalDequeSize(
    const absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>>& frame_map) {
  size_t total_size = 0;
  for (const auto& pair : frame_map) {
    total_size += pair.second.size();
  }
  return total_size;
}

TEST_F(MongoDBStitchFramesTest, VerifyOnetoOneStitching) {
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> reqs;
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> resps;

  // Add requests to map.
  reqs[1].push_back(CreateMongoDBFrame(0, 1, 0, false));
  reqs[3].push_back(CreateMongoDBFrame(2, 3, 0, false));
  reqs[5].push_back(CreateMongoDBFrame(4, 5, 0, false));
  reqs[7].push_back(CreateMongoDBFrame(6, 7, 0, false));
  reqs[9].push_back(CreateMongoDBFrame(8, 9, 0, false));
  reqs[11].push_back(CreateMongoDBFrame(10, 11, 0, false));
  reqs[13].push_back(CreateMongoDBFrame(12, 13, 0, false));
  reqs[15].push_back(CreateMongoDBFrame(14, 15, 0, false));

  // Add responses to map.
  resps[1].push_back(CreateMongoDBFrame(1, 2, 1, false));
  resps[3].push_back(CreateMongoDBFrame(3, 4, 3, false));
  resps[5].push_back(CreateMongoDBFrame(5, 6, 5, false));
  resps[7].push_back(CreateMongoDBFrame(7, 8, 7, false));
  resps[9].push_back(CreateMongoDBFrame(9, 10, 9, false));
  resps[11].push_back(CreateMongoDBFrame(11, 12, 11, false));
  resps[13].push_back(CreateMongoDBFrame(13, 14, 13, false));
  resps[15].push_back(CreateMongoDBFrame(15, 16, 15, false));

  // Add the order in which the transactions's streamID's were found.
  State state = {};
  state.transaction_stream_order.push_back(1);
  state.transaction_stream_order.push_back(3);
  state.transaction_stream_order.push_back(5);
  state.transaction_stream_order.push_back(7);
  state.transaction_stream_order.push_back(9);
  state.transaction_stream_order.push_back(11);
  state.transaction_stream_order.push_back(13);
  state.transaction_stream_order.push_back(15);

  RecordsWithErrorCount<mongodb::Record> result = mongodb::StitchFrames(&reqs, &resps, &state);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_THAT(result.records, SizeIs(8));
  EXPECT_EQ(totalDequeSize(reqs), 0);
  EXPECT_TRUE(areAllDequesEmpty(resps));
  EXPECT_THAT(state.transaction_stream_order, SizeIs(0));
}

TEST_F(MongoDBStitchFramesTest, VerifyOnetoNStitching) {
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> reqs;
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> resps;

  // Add requests to map.
  reqs[1].push_back(CreateMongoDBFrame(0, 1, 0, false));
  reqs[3].push_back(CreateMongoDBFrame(2, 3, 0, false));

  // Request frame corresponding to multi frame response message.
  reqs[5].push_back(CreateMongoDBFrame(4, 5, 0, false));

  reqs[9].push_back(CreateMongoDBFrame(8, 9, 0, false));
  reqs[11].push_back(CreateMongoDBFrame(10, 11, 0, false));
  reqs[13].push_back(CreateMongoDBFrame(12, 13, 0, false));
  reqs[15].push_back(CreateMongoDBFrame(14, 15, 0, false));
  reqs[17].push_back(CreateMongoDBFrame(16, 17, 0, false));

  // Add responses to map.
  resps[1].push_back(CreateMongoDBFrame(1, 2, 1, false));
  resps[3].push_back(CreateMongoDBFrame(3, 4, 3, false));

  // Multi frame response message.
  resps[5].push_back(CreateMongoDBFrame(5, 6, 5, true, "1"));
  resps[6].push_back(CreateMongoDBFrame(6, 7, 6, true, "2"));
  resps[7].push_back(CreateMongoDBFrame(7, 8, 7, false, "3"));

  resps[9].push_back(CreateMongoDBFrame(9, 10, 9, false));
  resps[11].push_back(CreateMongoDBFrame(11, 12, 11, false));
  resps[13].push_back(CreateMongoDBFrame(13, 14, 13, false));
  resps[15].push_back(CreateMongoDBFrame(15, 16, 15, false));
  resps[17].push_back(CreateMongoDBFrame(17, 18, 17, false));

  // Add the order in which the transactions's streamID's were found.
  State state = {};
  state.transaction_stream_order.push_back(1);
  state.transaction_stream_order.push_back(3);
  state.transaction_stream_order.push_back(5);
  state.transaction_stream_order.push_back(9);
  state.transaction_stream_order.push_back(11);
  state.transaction_stream_order.push_back(13);
  state.transaction_stream_order.push_back(15);
  state.transaction_stream_order.push_back(17);

  RecordsWithErrorCount<mongodb::Record> result = mongodb::StitchFrames(&reqs, &resps, &state);

  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records[2].resp.sections[0].documents[0], "1");
  EXPECT_EQ(result.records[2].resp.sections[1].documents[0], "2");
  EXPECT_EQ(result.records[2].resp.sections[2].documents[0], "3");
  EXPECT_THAT(result.records, SizeIs(8));

  EXPECT_EQ(totalDequeSize(reqs), 0);
  EXPECT_EQ(totalDequeSize(resps), 0);
  EXPECT_THAT(state.transaction_stream_order, SizeIs(0));
}

TEST_F(MongoDBStitchFramesTest, UnmatchedResponsesAreHandled) {
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> reqs;
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> resps;

  // Add requests to map.
  // Missing request frame
  reqs[2].push_back(CreateMongoDBFrame(1, 2, 0, false));

  // Add responses to map;
  resps[10].push_back(CreateMongoDBFrame(0, 1, 10, false));
  resps[2].push_back(CreateMongoDBFrame(2, 3, 2, false));

  // Add the order in which the transactions's streamID's were found.
  State state = {};
  state.transaction_stream_order.push_back(2);

  RecordsWithErrorCount<mongodb::Record> result = mongodb::StitchFrames(&reqs, &resps, &state);

  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 1);
  EXPECT_EQ(result.records[0].req.request_id, 2);
  EXPECT_EQ(result.records[0].resp.response_to, 2);

  EXPECT_EQ(totalDequeSize(reqs), 0);
  EXPECT_TRUE(areAllDequesEmpty(resps));
  EXPECT_THAT(state.transaction_stream_order, SizeIs(0));
}

TEST_F(MongoDBStitchFramesTest, UnmatchedRequestsAreCleanedUp) {
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> reqs;
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> resps;

  // Add requests to map.
  reqs[1].push_back(CreateMongoDBFrame(0, 1, 0, false));
  reqs[2].push_back(CreateMongoDBFrame(1, 2, 0, false));
  reqs[4].push_back(CreateMongoDBFrame(3, 4, 0, false));

  // Add responses to map.
  resps[2].push_back(CreateMongoDBFrame(2, 3, 2, false));
  resps[4].push_back(CreateMongoDBFrame(4, 5, 4, false));

  State state = {};
  state.transaction_stream_order.push_back(1);
  state.transaction_stream_order.push_back(2);
  state.transaction_stream_order.push_back(4);

  RecordsWithErrorCount<mongodb::Record> result = mongodb::StitchFrames(&reqs, &resps, &state);

  EXPECT_EQ(result.error_count, 0);
  EXPECT_THAT(result.records, SizeIs(2));
  EXPECT_EQ(result.records[0].req.request_id, 2);
  EXPECT_EQ(result.records[1].req.request_id, 4);

  EXPECT_TRUE(areAllDequesEmpty(reqs));
  EXPECT_TRUE(areAllDequesEmpty(resps));
  EXPECT_THAT(state.transaction_stream_order, SizeIs(0));
}

TEST_F(MongoDBStitchFramesTest, MissingHeadFrameInNResponses) {
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> reqs;
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> resps;

  // Add requests to map.
  reqs[1].push_back(CreateMongoDBFrame(0, 1, 0, false));
  reqs[6].push_back(CreateMongoDBFrame(5, 6, 0, false));

  // Add responses to map.
  // Missing head frame in the N responses
  resps[2].push_back(CreateMongoDBFrame(2, 3, 2, true));
  resps[3].push_back(CreateMongoDBFrame(3, 4, 3, false));
  resps[6].push_back(CreateMongoDBFrame(6, 7, 6, false));

  State state = {};
  state.transaction_stream_order.push_back(1);
  state.transaction_stream_order.push_back(6);

  RecordsWithErrorCount<mongodb::Record> result = mongodb::StitchFrames(&reqs, &resps, &state);

  EXPECT_EQ(result.error_count, 3);
  EXPECT_EQ(result.records.size(), 1);

  EXPECT_EQ(totalDequeSize(reqs), 1);
  EXPECT_TRUE(areAllDequesEmpty(resps));
  EXPECT_THAT(state.transaction_stream_order, SizeIs(0));
}

TEST_F(MongoDBStitchFramesTest, MissingFrameInNResponses) {
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> reqs;
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> resps;

  // Add requests to map.
  reqs[1].push_back(CreateMongoDBFrame(0, 1, 0, false));
  reqs[6].push_back(CreateMongoDBFrame(5, 6, 0, false));

  // Add responses to map.
  resps[1].push_back(CreateMongoDBFrame(1, 2, 1, true));
  resps[2].push_back(CreateMongoDBFrame(2, 3, 2, true));
  // Missing middle frame in the N responses.
  resps[4].push_back(CreateMongoDBFrame(4, 5, 4, false));
  resps[6].push_back(CreateMongoDBFrame(6, 7, 6, false));

  State state = {};
  state.transaction_stream_order.push_back(1);
  state.transaction_stream_order.push_back(6);

  RecordsWithErrorCount<mongodb::Record> result = mongodb::StitchFrames(&reqs, &resps, &state);

  EXPECT_EQ(result.error_count, 2);
  EXPECT_EQ(result.records.size(), 2);

  EXPECT_EQ(totalDequeSize(reqs), 0);
  EXPECT_TRUE(areAllDequesEmpty(resps));
  EXPECT_THAT(state.transaction_stream_order, SizeIs(0));
}

TEST_F(MongoDBStitchFramesTest, MissingTailFrameInNResponses) {
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> reqs;
  absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>> resps;

  // Add requests to map.
  reqs[1].push_back(CreateMongoDBFrame(0, 1, 0, false));
  reqs[6].push_back(CreateMongoDBFrame(5, 6, 0, false));

  // Add responses to map.
  resps[1].push_back(CreateMongoDBFrame(1, 2, 1, true));
  resps[2].push_back(CreateMongoDBFrame(2, 3, 2, true));
  resps[3].push_back(CreateMongoDBFrame(3, 4, 3, true));
  // Missing tail frame in the N responses
  resps[6].push_back(CreateMongoDBFrame(6, 7, 6, false));

  State state = {};
  state.transaction_stream_order.push_back(1);
  state.transaction_stream_order.push_back(6);

  RecordsWithErrorCount<mongodb::Record> result = mongodb::StitchFrames(&reqs, &resps, &state);

  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 2);

  EXPECT_EQ(totalDequeSize(reqs), 0);
  EXPECT_TRUE(areAllDequesEmpty(resps));
  EXPECT_THAT(state.transaction_stream_order, SizeIs(0));
}

}  // namespace mongodb
}  // namespace protocols
}  // namespace stirling
}  // namespace px