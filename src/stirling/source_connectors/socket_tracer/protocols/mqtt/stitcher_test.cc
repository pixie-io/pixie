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

#include <absl/container/flat_hash_map.h>
#include <gtest/gtest.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/test_utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {

using testutils::CreateFrame;

TEST(MqttStitcherTest, EmptyInputs) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);
}

TEST(MqttStitcherTest, OnlyRequests) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;

  Message connect_frame, pingreq_frame;
  connect_frame = CreateFrame(kRequest, 1, 0, 0);
  pingreq_frame = CreateFrame(kRequest, 12, 0, 0);

  int t = 0;
  connect_frame.timestamp_ns = ++t;
  pingreq_frame.timestamp_ns = ++t;
  req_map[0].push_back(connect_frame);
  req_map[0].push_back(pingreq_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 2);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);
}

TEST(MqttStitcherTest, OnlyResponses) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;

  Message connack_frame, pingresp_frame;
  connack_frame = CreateFrame(kResponse, 2, 0, 0);
  pingresp_frame = CreateFrame(kResponse, 13, 0, 0);

  int t = 0;
  connack_frame.timestamp_ns = ++t;
  pingresp_frame.timestamp_ns = ++t;
  resp_map[0].push_back(connack_frame);
  resp_map[0].push_back(pingresp_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(result.error_count, 2);
  EXPECT_EQ(result.records.size(), 0);
}

TEST(MqttStitcherTest, MissingResponse) {
  // Test for packet stitching for packets that do not have packet identifiers
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;

  Message connect_frame, connack_frame, pingreq_frame, pingresp_frame, pub1_frame, pub1_pid3_frame,
      puback_frame, puback_pid3_frame, pub2_frame, pub2_pid2_frame, pubrec_frame, pubrec_pid2_frame,
      pubrel_frame, pubrel_pid2_frame, pubcomp_frame, pubcomp_pid2_frame, sub_frame, suback_frame,
      unsub_frame, unsuback_frame;
  connect_frame = CreateFrame(kRequest, 1, 0, 0);
  connack_frame = CreateFrame(kResponse, 2, 0, 0);
  pingreq_frame = CreateFrame(kRequest, 12, 0, 0);
  pingresp_frame = CreateFrame(kResponse, 13, 0, 0);
  pub1_frame = CreateFrame(kRequest, 3, 1, 1);
  pub1_pid3_frame = CreateFrame(kRequest, 3, 3, 1);
  pub2_frame = CreateFrame(kRequest, 3, 1, 2);
  pub2_pid2_frame = CreateFrame(kRequest, 3, 2, 2);
  puback_frame = CreateFrame(kResponse, 4, 1, 0);
  puback_pid3_frame = CreateFrame(kResponse, 4, 3, 0);
  pubrec_frame = CreateFrame(kResponse, 5, 0, 0);
  pubrec_pid2_frame = CreateFrame(kResponse, 5, 2, 0);
  pubrel_frame = CreateFrame(kRequest, 6, 0, 0);
  pubrel_pid2_frame = CreateFrame(kRequest, 6, 2, 0);
  pubcomp_frame = CreateFrame(kResponse, 7, 0, 0);
  pubcomp_pid2_frame = CreateFrame(kResponse, 7, 2, 0);
  sub_frame = CreateFrame(kRequest, 8, 0, 0);
  suback_frame = CreateFrame(kResponse, 9, 0, 0);
  unsub_frame = CreateFrame(kRequest, 10, 0, 0);
  unsuback_frame = CreateFrame(kResponse, 11, 0, 0);

  int t = 0;
  connect_frame.timestamp_ns = ++t;
  pingreq_frame.timestamp_ns = ++t;
  pingresp_frame.timestamp_ns = ++t;
  req_map[0].push_back(connect_frame);
  req_map[0].push_back(pingreq_frame);
  resp_map[0].push_back(pingresp_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 1);
  req_map.clear();
  resp_map.clear();

  // Missing response before the next response
  t = 0;
  sub_frame.timestamp_ns = ++t;
  pub1_frame.timestamp_ns = ++t;
  puback_frame.timestamp_ns = ++t;
  unsub_frame.timestamp_ns = ++t;
  unsuback_frame.timestamp_ns = ++t;
  req_map[1].push_back(sub_frame);
  req_map[1].push_back(pub1_frame);
  req_map[2].push_back(unsub_frame);
  resp_map[1].push_back(puback_frame);
  resp_map[2].push_back(unsuback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 2);
  req_map.clear();
  resp_map.clear();

  // Response not yet received for a particular packet identifier
  t = 0;
  sub_frame.timestamp_ns = ++t;
  pub1_frame.timestamp_ns = ++t;
  unsub_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  unsuback_frame.timestamp_ns = ++t;
  req_map[1].push_back(sub_frame);
  req_map[1].push_back(pub1_frame);
  req_map[2].push_back(unsub_frame);
  resp_map[1].push_back(suback_frame);
  resp_map[2].push_back(unsuback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 1);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
  req_map.clear();
  resp_map.clear();

  t = 0;
  sub_frame.timestamp_ns = ++t;
  pub1_frame.timestamp_ns = ++t;
  unsub_frame.timestamp_ns = ++t;
  unsuback_frame.timestamp_ns = ++t;
  req_map[1].push_back(sub_frame);
  req_map[1].push_back(pub1_frame);
  req_map[2].push_back(unsub_frame);
  resp_map[2].push_back(unsuback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 2);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);
  req_map.clear();
  resp_map.clear();

  t = 0;
  sub_frame.timestamp_ns = ++t;
  pub1_frame.timestamp_ns = ++t;
  unsub_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  req_map[1].push_back(sub_frame);
  req_map[1].push_back(pub1_frame);
  req_map[2].push_back(unsub_frame);
  resp_map[1].push_back(suback_frame);
  resp_map[1].push_back(puback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 1);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
}

TEST(MqttStitcherTest, MissingRequest) {
  // Test for packet stitching for packets that do not have packet identifiers
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;

  Message connect_frame, connack_frame, pingresp_frame;
  connect_frame = CreateFrame(kRequest, 1, 0, 0);
  connack_frame = CreateFrame(kResponse, 2, 0, 0);
  pingresp_frame = CreateFrame(kResponse, 13, 0, 0);

  int t = 0;
  connect_frame.timestamp_ns = ++t;
  connack_frame.timestamp_ns = ++t;
  pingresp_frame.timestamp_ns = ++t;
  req_map[0].push_back(connect_frame);
  resp_map[0].push_back(connack_frame);
  resp_map[0].push_back(pingresp_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 1);
}

TEST(MqttStitcherTest, InOrderMatching) {
  // Test for packet stitching for packets that do not have packet identifiers
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;

  Message connect_frame, connack_frame, pingreq_frame, pingresp_frame, pub1_frame, pub1_pid3_frame,
      puback_frame, puback_pid3_frame, pub2_frame, pub2_pid2_frame, pubrec_frame, pubrec_pid2_frame,
      pubrel_frame, pubrel_pid2_frame, pubcomp_frame, pubcomp_pid2_frame, sub_frame, suback_frame,
      unsub_frame, unsuback_frame;
  connect_frame = CreateFrame(kRequest, 1, 0, 0);
  connack_frame = CreateFrame(kResponse, 2, 0, 0);
  pingreq_frame = CreateFrame(kRequest, 12, 0, 0);
  pingresp_frame = CreateFrame(kResponse, 13, 0, 0);
  pub1_frame = CreateFrame(kRequest, 3, 1, 1);
  pub1_pid3_frame = CreateFrame(kRequest, 3, 3, 1);
  pub2_frame = CreateFrame(kRequest, 3, 1, 2);
  pub2_pid2_frame = CreateFrame(kRequest, 3, 2, 2);
  puback_frame = CreateFrame(kResponse, 4, 1, 0);
  puback_pid3_frame = CreateFrame(kResponse, 4, 3, 0);
  pubrec_frame = CreateFrame(kResponse, 5, 0, 0);
  pubrec_pid2_frame = CreateFrame(kResponse, 5, 2, 0);
  pubrel_frame = CreateFrame(kRequest, 6, 0, 0);
  pubrel_pid2_frame = CreateFrame(kRequest, 6, 2, 0);
  pubcomp_frame = CreateFrame(kResponse, 7, 0, 0);
  pubcomp_pid2_frame = CreateFrame(kResponse, 7, 2, 0);
  sub_frame = CreateFrame(kRequest, 8, 0, 0);
  suback_frame = CreateFrame(kResponse, 9, 0, 0);
  unsub_frame = CreateFrame(kRequest, 10, 0, 0);
  unsuback_frame = CreateFrame(kResponse, 11, 0, 0);

  int t = 0;
  connect_frame.timestamp_ns = ++t;
  connack_frame.timestamp_ns = ++t;
  pingreq_frame.timestamp_ns = ++t;
  pingresp_frame.timestamp_ns = ++t;
  req_map[0].push_back(connect_frame);
  resp_map[0].push_back(connack_frame);
  req_map[0].push_back(pingreq_frame);
  resp_map[0].push_back(pingresp_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
  req_map.clear();
  resp_map.clear();

  t = 0;
  sub_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  pub1_frame.timestamp_ns = ++t;
  puback_frame.timestamp_ns = ++t;
  unsub_frame.timestamp_ns = ++t;
  unsuback_frame.timestamp_ns = ++t;
  req_map[1].push_back(sub_frame);
  req_map[1].push_back(pub1_frame);
  req_map[2].push_back(unsub_frame);
  resp_map[1].push_back(suback_frame);
  resp_map[1].push_back(puback_frame);
  resp_map[2].push_back(unsuback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 3);
  req_map.clear();
  resp_map.clear();

  t = 0;
  sub_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  pub2_frame.timestamp_ns = ++t;
  pubrec_frame.timestamp_ns = ++t;
  pubrel_frame.timestamp_ns = ++t;
  pubcomp_frame.timestamp_ns = ++t;
  unsub_frame.timestamp_ns = ++t;
  unsuback_frame.timestamp_ns = ++t;
  req_map[1].push_back(sub_frame);
  req_map[1].push_back(pub2_frame);
  req_map[1].push_back(pubrel_frame);
  req_map[2].push_back(unsub_frame);
  resp_map[1].push_back(suback_frame);
  resp_map[1].push_back(pubrec_frame);
  resp_map[1].push_back(pubcomp_frame);
  resp_map[2].push_back(unsuback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 4);
  req_map.clear();
  resp_map.clear();
}

TEST(MqttStitcherTest, OutOfOrderMatching) {
  // Test for packet stitching for packets that do not have packet identifiers
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;

  Message connect_frame, connack_frame, pingreq_frame, pingresp_frame, pub1_frame, pub1_pid3_frame,
      puback_frame, puback_pid3_frame, pub2_frame, pub2_pid2_frame, pubrec_frame, pubrec_pid2_frame,
      pubrel_frame, pubrel_pid2_frame, pubcomp_frame, pubcomp_pid2_frame, sub_frame, suback_frame,
      unsub_frame, unsuback_frame;

  connect_frame = CreateFrame(kRequest, 1, 0, 0);
  connack_frame = CreateFrame(kResponse, 2, 0, 0);
  pingreq_frame = CreateFrame(kRequest, 12, 0, 0);
  pingresp_frame = CreateFrame(kResponse, 13, 0, 0);
  pub1_frame = CreateFrame(kRequest, 3, 1, 1);
  pub1_pid3_frame = CreateFrame(kRequest, 3, 3, 1);
  pub2_frame = CreateFrame(kRequest, 3, 1, 2);
  pub2_pid2_frame = CreateFrame(kRequest, 3, 2, 2);
  puback_frame = CreateFrame(kResponse, 4, 1, 0);
  puback_pid3_frame = CreateFrame(kResponse, 4, 3, 0);
  pubrec_frame = CreateFrame(kResponse, 5, 0, 0);
  pubrec_pid2_frame = CreateFrame(kResponse, 5, 2, 0);
  pubrel_frame = CreateFrame(kRequest, 6, 0, 0);
  pubrel_pid2_frame = CreateFrame(kRequest, 6, 2, 0);
  pubcomp_frame = CreateFrame(kResponse, 7, 0, 0);
  pubcomp_pid2_frame = CreateFrame(kResponse, 7, 2, 0);
  sub_frame = CreateFrame(kRequest, 8, 0, 0);
  suback_frame = CreateFrame(kResponse, 9, 0, 0);
  unsub_frame = CreateFrame(kRequest, 10, 0, 0);
  unsuback_frame = CreateFrame(kResponse, 11, 0, 0);

  int t = 0;
  connect_frame.timestamp_ns = ++t;
  pingreq_frame.timestamp_ns = ++t;
  pingresp_frame.timestamp_ns = ++t;
  connack_frame.timestamp_ns = ++t;
  req_map[0].push_back(connect_frame);
  resp_map[0].push_back(connack_frame);
  req_map[0].push_back(pingreq_frame);
  resp_map[0].push_back(pingresp_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);

  // Delayed subscribe response
  t = 0;
  sub_frame.timestamp_ns = ++t;
  pub2_frame.timestamp_ns = ++t;
  pubrec_frame.timestamp_ns = ++t;
  pubrel_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  pubcomp_frame.timestamp_ns = ++t;
  unsub_frame.timestamp_ns = ++t;
  unsuback_frame.timestamp_ns = ++t;
  req_map[1].push_back(sub_frame);
  req_map[1].push_back(pub2_frame);
  req_map[1].push_back(pubrel_frame);
  req_map[2].push_back(unsub_frame);
  resp_map[1].push_back(pubrec_frame);
  resp_map[1].push_back(suback_frame);
  resp_map[1].push_back(pubcomp_frame);
  resp_map[2].push_back(unsuback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 4);
  req_map.clear();
  resp_map.clear();

  // Delayed responses with interleaved requests
  t = 0;
  pub1_frame.timestamp_ns = ++t;
  pub2_pid2_frame.timestamp_ns = ++t;
  pub1_pid3_frame.timestamp_ns = ++t;
  puback_frame.timestamp_ns = ++t;
  pubrec_pid2_frame.timestamp_ns = ++t;
  pubrel_pid2_frame.timestamp_ns = ++t;
  pubcomp_pid2_frame.timestamp_ns = ++t;
  puback_pid3_frame.timestamp_ns = ++t;
  req_map[1].push_back(pub1_frame);
  req_map[2].push_back(pub2_pid2_frame);
  req_map[3].push_back(pub1_pid3_frame);
  req_map[2].push_back(pubrel_pid2_frame);
  resp_map[1].push_back(puback_frame);
  resp_map[2].push_back(pubrec_pid2_frame);
  resp_map[2].push_back(pubcomp_pid2_frame);
  resp_map[3].push_back(puback_pid3_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 4);
  req_map.clear();
  resp_map.clear();
}

TEST(MqttStitcherTest, DummyResponseStitching) {
  // Test for requests that do not have responses
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;

  Message pub0_frame, disconnect_frame, auth_frame, connect_frame, connack_frame;
  pub0_frame = CreateFrame(kRequest, 3, 0, 0);         // PUBLISH with QoS 0
  disconnect_frame = CreateFrame(kRequest, 14, 0, 0);  // DISCONNECT
  auth_frame = CreateFrame(kRequest, 15, 0, 0);        // AUTH
  connect_frame = CreateFrame(kRequest, 1, 0, 0);      // CONNECT
  connack_frame = CreateFrame(kResponse, 2, 0, 0);     // CONNACK

  int t = 0;
  pub0_frame.timestamp_ns = ++t;
  disconnect_frame.timestamp_ns = ++t;
  auth_frame.timestamp_ns = ++t;
  req_map[0].push_back(pub0_frame);
  req_map[0].push_back(disconnect_frame);
  req_map[0].push_back(auth_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 3);
  req_map.clear();
  resp_map.clear();

  t = 0;
  pub0_frame.timestamp_ns = ++t;
  connect_frame.timestamp_ns = ++t;
  disconnect_frame.timestamp_ns = ++t;
  auth_frame.timestamp_ns = ++t;
  connack_frame.timestamp_ns = ++t;
  req_map[0].push_back(pub0_frame);
  req_map[0].push_back(connect_frame);
  req_map[0].push_back(disconnect_frame);
  req_map[0].push_back(auth_frame);
  resp_map[0].push_back(connack_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 4);
}

TEST(MqttStitcherTest, DuplicateRequests) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  Message pub1_frame_1, pub1_frame_2, pub2_frame_1, pub2_frame_2, puback_frame, pubrec_frame,
      subscribe_frame, suback_frame;
  pub1_frame_1 = CreateFrame(kRequest, 3, 1, 1);
  pub1_frame_2 = CreateFrame(kRequest, 3, 1, 1);
  pub1_frame_2.dup = true;
  pub2_frame_1 = CreateFrame(kRequest, 3, 1, 2);
  pub2_frame_2 = CreateFrame(kRequest, 3, 1, 2);
  pub2_frame_2.dup = true;
  puback_frame = CreateFrame(kResponse, 4, 1, 0);
  pubrec_frame = CreateFrame(kResponse, 5, 1, 0);
  subscribe_frame = CreateFrame(kRequest, 8, 1, 0);
  suback_frame = CreateFrame(kResponse, 9, 1, 0);

  // Duplicate publish (QOS 1) which has been answered
  int t = 0;
  pub1_frame_1.timestamp_ns = ++t;
  pub1_frame_2.timestamp_ns = ++t;
  puback_frame.timestamp_ns = ++t;
  req_map[1].push_back(pub1_frame_1);
  req_map[1].push_back(pub1_frame_2);
  resp_map[1].push_back(puback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);
  req_map.clear();
  resp_map.clear();

  // Adding a subsequent request and response after duplicate PUBLISH (QOS 1) request
  t = 0;
  pub1_frame_1.timestamp_ns = ++t;
  pub1_frame_2.timestamp_ns = ++t;
  puback_frame.timestamp_ns = ++t;
  subscribe_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  req_map[1].push_back(pub1_frame_1);
  req_map[1].push_back(pub1_frame_2);
  req_map[1].push_back(subscribe_frame);
  resp_map[1].push_back(puback_frame);
  resp_map[1].push_back(suback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
  req_map.clear();
  resp_map.clear();

  // Unanswered duplicate PUBLISH (QOS 1)
  t = 0;
  pub1_frame_1.timestamp_ns = ++t;
  subscribe_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  pub1_frame_2.timestamp_ns = ++t;
  req_map[1].push_back(pub1_frame_1);
  req_map[1].push_back(subscribe_frame);
  resp_map[1].push_back(suback_frame);
  req_map[1].push_back(pub1_frame_2);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 1);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 1);
  req_map.clear();
  resp_map.clear();

  // Duplicate publish (QOS 2) which has been answered
  t = 0;
  pub2_frame_1.timestamp_ns = ++t;
  pub2_frame_2.timestamp_ns = ++t;
  pubrec_frame.timestamp_ns = ++t;
  req_map[1].push_back(pub2_frame_1);
  req_map[1].push_back(pub2_frame_2);
  resp_map[1].push_back(pubrec_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);
  req_map.clear();
  resp_map.clear();

  // Adding a subsequent request and response after duplicate PUBLISH (QOS 2) request
  t = 0;
  pub2_frame_1.timestamp_ns = ++t;
  pub2_frame_2.timestamp_ns = ++t;
  pubrec_frame.timestamp_ns = ++t;
  subscribe_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  req_map[1].push_back(pub2_frame_1);
  req_map[1].push_back(pub2_frame_2);
  req_map[1].push_back(subscribe_frame);
  resp_map[1].push_back(pubrec_frame);
  resp_map[1].push_back(suback_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
  req_map.clear();
  resp_map.clear();

  // Unanswered duplicate PUBLISH (QOS 2)
  t = 0;
  pub2_frame_1.timestamp_ns = ++t;
  subscribe_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  pub2_frame_2.timestamp_ns = ++t;
  req_map[1].push_back(pub2_frame_1);
  req_map[1].push_back(subscribe_frame);
  resp_map[1].push_back(suback_frame);
  req_map[1].push_back(pub2_frame_2);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 1);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 1);
  req_map.clear();
  resp_map.clear();

  // Duplicate publish (QOS 1) and publish (QOS 2) which have been answered
  t = 0;
  pub1_frame_1.timestamp_ns = ++t;
  pub1_frame_2.timestamp_ns = ++t;
  puback_frame.timestamp_ns = ++t;
  pub2_frame_1.timestamp_ns = ++t;
  pub1_frame_2.timestamp_ns = ++t;
  pubrec_frame.timestamp_ns = ++t;
  req_map[1].push_back(pub1_frame_1);
  req_map[1].push_back(pub1_frame_2);
  resp_map[1].push_back(puback_frame);
  req_map[1].push_back(pub2_frame_1);
  req_map[1].push_back(pub2_frame_2);
  resp_map[1].push_back(pubrec_frame);
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
  req_map.clear();
  resp_map.clear();
}

}  // namespace mqtt
}  // namespace protocols
}  // namespace stirling
}  // namespace px
