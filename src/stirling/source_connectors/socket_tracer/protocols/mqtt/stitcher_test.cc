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
  StateWrapper state = {.send = {}, .recv = {}};

  result = StitchFrames(&req_map, &resp_map, &state);

  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);
}

TEST(MqttStitcherTest, OnlyRequests) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state = {.send = {}, .recv = {}};

  Message connect_frame, pingreq_frame;
  connect_frame = CreateFrame(kRequest, MqttControlPacketType::CONNECT, 0, 0);
  pingreq_frame = CreateFrame(kRequest, MqttControlPacketType::CONNACK, 0, 0);

  int t = 0;
  connect_frame.timestamp_ns = ++t;
  pingreq_frame.timestamp_ns = ++t;

  req_map[0].push_back(connect_frame);
  req_map[0].push_back(pingreq_frame);

  result = StitchFrames(&req_map, &resp_map, &state);

  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 2);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);
}

TEST(MqttStitcherTest, OnlyResponses) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state = {.send = {}, .recv = {}};

  Message connack_frame, pingresp_frame;
  connack_frame = CreateFrame(kResponse, MqttControlPacketType::CONNACK, 0, 0);
  pingresp_frame = CreateFrame(kResponse, MqttControlPacketType::PINGRESP, 0, 0);

  int t = 0;
  connack_frame.timestamp_ns = ++t;
  pingresp_frame.timestamp_ns = ++t;

  resp_map[0].push_back(connack_frame);
  resp_map[0].push_back(pingresp_frame);

  result = StitchFrames(&req_map, &resp_map, &state);

  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(result.error_count, 2);
  EXPECT_EQ(result.records.size(), 0);
}

TEST(MqttStitcherTest, MissingResponseBeforeNextResponse) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state = {.send = {}, .recv = {}};

  Message pub1_frame, puback_frame, sub_frame, suback_frame, unsub_frame, unsuback_frame;
  pub1_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 1);
  puback_frame = CreateFrame(kResponse, MqttControlPacketType::PUBACK, 1, 0);
  sub_frame = CreateFrame(kRequest, MqttControlPacketType::SUBSCRIBE, 0, 0);
  unsub_frame = CreateFrame(kRequest, MqttControlPacketType::UNSUBSCRIBE, 0, 0);
  unsuback_frame = CreateFrame(kResponse, MqttControlPacketType::UNSUBACK, 0, 0);

  int t = 0;
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

  // Update the state for PUBLISH packet ID 1 and QOS 1
  state.send[std::tuple<uint32_t, uint32_t>(1, 1)] = 0;

  result = StitchFrames(&req_map, &resp_map, &state);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 2);
}

TEST(MqttStitcherTest, MissingResponseTailEnd) {
  // Response not yet received for a particular packet identifier
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state = {.send = {}, .recv = {}};

  Message pub1_frame, sub_frame, suback_frame, unsub_frame, unsuback_frame;
  pub1_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 1);
  sub_frame = CreateFrame(kRequest, MqttControlPacketType::SUBSCRIBE, 0, 0);
  suback_frame = CreateFrame(kResponse, MqttControlPacketType::SUBACK, 0, 0);
  unsub_frame = CreateFrame(kRequest, MqttControlPacketType::UNSUBSCRIBE, 0, 0);
  unsuback_frame = CreateFrame(kResponse, MqttControlPacketType::UNSUBACK, 0, 0);

  int t = 0;
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

  // Update the state for PUBLISH packet ID 1 and QOS 1
  state.send[std::tuple<uint32_t, uint32_t>(1, 1)] = 0;

  result = StitchFrames(&req_map, &resp_map, &state);

  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 1);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
  req_map.clear();
}

TEST(MqttStitcherTest, MissingRequest) {
  // Test for packet stitching for packets that do not have packet identifiers
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state = {.send = {}, .recv = {}};

  Message connect_frame, connack_frame, pingresp_frame;
  connect_frame = CreateFrame(kRequest, MqttControlPacketType::CONNECT, 0, 0);
  connack_frame = CreateFrame(kResponse, MqttControlPacketType::CONNACK, 0, 0);
  pingresp_frame = CreateFrame(kResponse, MqttControlPacketType::PINGRESP, 0, 0);

  int t = 0;
  connect_frame.timestamp_ns = ++t;
  connack_frame.timestamp_ns = ++t;
  pingresp_frame.timestamp_ns = ++t;

  req_map[0].push_back(connect_frame);

  resp_map[0].push_back(connack_frame);
  resp_map[0].push_back(pingresp_frame);

  result = StitchFrames(&req_map, &resp_map, &state);

  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 1);
}

TEST(MqttStitcherTest, InOrderMatching) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state = {.send = {}, .recv = {}};

  // Establishment of connection, ping requests and responses, and three publish requests (qos 1,
  // qos 2 and qos 1) with increasing packet identifiers (since they are sent before their responses
  // are received)
  Message connect_frame, connack_frame, pingreq_frame, pingresp_frame, pub1_frame, pub1_pid3_frame,
      puback_frame, puback_pid3_frame, pub2_pid2_frame, pubrec_pid2_frame, pubrel_pid2_frame,
      pubcomp_pid2_frame, sub_frame, suback_frame, unsub_frame, unsuback_frame, auth_frame_server,
      auth_frame_client;
  connect_frame = CreateFrame(kRequest, MqttControlPacketType::CONNECT, 0, 0);
  connack_frame = CreateFrame(kResponse, MqttControlPacketType::CONNACK, 0, 0);
  pingreq_frame = CreateFrame(kRequest, MqttControlPacketType::PINGREQ, 0, 0);
  pingresp_frame = CreateFrame(kResponse, MqttControlPacketType::PINGRESP, 0, 0);
  pub1_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 1);
  pub1_pid3_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 3, 1);
  pub2_pid2_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 2, 2);
  puback_frame = CreateFrame(kResponse, MqttControlPacketType::PUBACK, 1, 0);
  puback_pid3_frame = CreateFrame(kResponse, MqttControlPacketType::PUBACK, 3, 0);
  pubrec_pid2_frame = CreateFrame(kResponse, MqttControlPacketType::PUBREC, 2, 0);
  pubrel_pid2_frame = CreateFrame(kRequest, MqttControlPacketType::PUBREL, 2, 0);
  pubcomp_pid2_frame = CreateFrame(kResponse, MqttControlPacketType::PUBCOMP, 2, 0);
  sub_frame = CreateFrame(kRequest, MqttControlPacketType::SUBSCRIBE, 0, 0);
  suback_frame = CreateFrame(kResponse, MqttControlPacketType::SUBACK, 0, 0);
  unsub_frame = CreateFrame(kRequest, MqttControlPacketType::UNSUBSCRIBE, 0, 0);
  unsuback_frame = CreateFrame(kResponse, MqttControlPacketType::UNSUBACK, 0, 0);

  int t = 0;
  connect_frame.timestamp_ns = ++t;
  connack_frame.timestamp_ns = ++t;
  pingreq_frame.timestamp_ns = ++t;
  pingresp_frame.timestamp_ns = ++t;
  sub_frame.timestamp_ns = ++t;
  suback_frame.timestamp_ns = ++t;
  pub1_frame.timestamp_ns = ++t;
  pub2_pid2_frame.timestamp_ns = ++t;
  pub1_pid3_frame.timestamp_ns = ++t;
  puback_frame.timestamp_ns = ++t;
  pubrec_pid2_frame.timestamp_ns = ++t;
  puback_pid3_frame.timestamp_ns = ++t;
  pubrel_pid2_frame.timestamp_ns = ++t;
  unsub_frame.timestamp_ns = ++t;
  pubcomp_pid2_frame.timestamp_ns = ++t;
  unsuback_frame.timestamp_ns = ++t;
  auth_frame_server.timestamp_ns = ++t;
  auth_frame_client.timestamp_ns = ++t;

  req_map[1].push_back(connect_frame);
  req_map[1].push_back(pingreq_frame);
  req_map[1].push_back(sub_frame);
  req_map[1].push_back(pub1_frame);
  req_map[2].push_back(pub2_pid2_frame);
  req_map[3].push_back(pub1_pid3_frame);
  req_map[2].push_back(pubrel_pid2_frame);
  req_map[2].push_back(unsub_frame);

  resp_map[1].push_back(connack_frame);
  resp_map[1].push_back(pingresp_frame);
  resp_map[1].push_back(suback_frame);
  resp_map[1].push_back(puback_frame);
  resp_map[2].push_back(pubrec_pid2_frame);
  resp_map[3].push_back(puback_pid3_frame);
  resp_map[2].push_back(pubcomp_pid2_frame);
  resp_map[2].push_back(unsuback_frame);

  // Update the state for PUBLISH packet ID 1 and QOS 1, packet ID 2 and QOS 2, packet ID 3 and QOS
  // 1
  state.send[std::tuple<uint32_t, uint32_t>(1, 1)] = 0;
  state.send[std::tuple<uint32_t, uint32_t>(2, 2)] = 0;
  state.send[std::tuple<uint32_t, uint32_t>(2, 1)] = 0;

  result = StitchFrames(&req_map, &resp_map, &state);

  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 8);
}

TEST(MqttStitcherTest, OutOfOrderMatching) {
  // Test for packet stitching for packets that do not have packet identifiers
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state = {.send = {}, .recv = {}};

  // Delayed response for PUBLISH (PID 3, QOS 1) and SUBSCRIBE (PID 4) (Delayed meaning some the
  // response for this request comes after the responses for later requests)
  Message pub1_pid_1_frame, pub1_pid3_frame, puback_pid1_frame, puback_pid3_frame, pub2_pid2_frame,
      pubrec_pid2_frame, pubrel_pid2_frame, pubcomp_pid2_frame, sub_pid4_frame, suback_pid4_frame;

  pub1_pid_1_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 1);
  pub1_pid3_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 3, 1);
  pub2_pid2_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 2, 2);
  puback_pid1_frame = CreateFrame(kResponse, MqttControlPacketType::PUBACK, 1, 0);
  puback_pid3_frame = CreateFrame(kResponse, MqttControlPacketType::PUBACK, 3, 0);
  pubrec_pid2_frame = CreateFrame(kResponse, MqttControlPacketType::PUBREC, 2, 0);
  pubrel_pid2_frame = CreateFrame(kRequest, MqttControlPacketType::PUBREL, 2, 0);
  pubcomp_pid2_frame = CreateFrame(kResponse, MqttControlPacketType::PUBCOMP, 2, 0);
  sub_pid4_frame = CreateFrame(kRequest, MqttControlPacketType::SUBSCRIBE, 4, 0);
  suback_pid4_frame = CreateFrame(kResponse, MqttControlPacketType::SUBACK, 4, 0);

  // Delayed responses with interleaved requests
  int t = 0;
  pub1_pid_1_frame.timestamp_ns = ++t;
  pub2_pid2_frame.timestamp_ns = ++t;
  pub1_pid3_frame.timestamp_ns = ++t;
  sub_pid4_frame.timestamp_ns = ++t;
  puback_pid1_frame.timestamp_ns = ++t;
  pubrec_pid2_frame.timestamp_ns = ++t;
  pubrel_pid2_frame.timestamp_ns = ++t;
  pubcomp_pid2_frame.timestamp_ns = ++t;
  puback_pid3_frame.timestamp_ns = ++t;
  suback_pid4_frame.timestamp_ns = ++t;

  req_map[1].push_back(pub1_pid_1_frame);
  req_map[2].push_back(pub2_pid2_frame);
  req_map[3].push_back(pub1_pid3_frame);
  req_map[2].push_back(pubrel_pid2_frame);
  resp_map[1].push_back(puback_pid1_frame);
  resp_map[2].push_back(pubrec_pid2_frame);
  resp_map[2].push_back(pubcomp_pid2_frame);
  resp_map[3].push_back(puback_pid3_frame);

  // Update the state for PUBLISH packet ID 1 and QOS 1, packet ID 2 and QOS 2, packet ID 3 and QOS
  // 1
  state.send[std::tuple<uint32_t, uint32_t>(1, 1)] = 0;
  state.send[std::tuple<uint32_t, uint32_t>(2, 2)] = 0;
  state.send[std::tuple<uint32_t, uint32_t>(3, 1)] = 0;

  result = StitchFrames(&req_map, &resp_map, &state);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 4);
}

TEST(MqttStitcherTest, DummyResponseStitching) {
  // Test for requests that do not have responses
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state = {.send = {}, .recv = {}};

  Message pub0_frame, disconnect_frame, connect_frame, connack_frame;
  pub0_frame = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 0, 0);  // PUBLISH with QoS 0
  disconnect_frame = CreateFrame(kRequest, MqttControlPacketType::DISCONNECT, 0, 0);  // DISCONNECT

  int t = 0;
  pub0_frame.timestamp_ns = ++t;
  disconnect_frame.timestamp_ns = ++t;

  req_map[0].push_back(pub0_frame);
  req_map[0].push_back(disconnect_frame);

  result = StitchFrames(&req_map, &resp_map, &state);

  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
}

TEST(MqttStitcherTest, DuplicateAnsweredRequests) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state;
  state.send = {};
  state.recv = {};

  Message pub1_frame_1, pub1_frame_2, pub2_frame_1, pub2_frame_2, puback_frame, pubrec_frame,
      subscribe_frame, suback_frame;
  pub1_frame_1 = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 1);
  pub1_frame_2 = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 1);
  pub1_frame_2.dup = true;
  pub2_frame_1 = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 2);
  pub2_frame_2 = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 2);
  pub2_frame_2.dup = true;
  puback_frame = CreateFrame(kResponse, MqttControlPacketType::PUBACK, 1, 0);
  pubrec_frame = CreateFrame(kResponse, MqttControlPacketType::PUBREC, 1, 0);

  int t = 0;
  pub1_frame_1.timestamp_ns = ++t;
  pub1_frame_2.timestamp_ns = ++t;
  puback_frame.timestamp_ns = ++t;
  pub2_frame_1.timestamp_ns = ++t;
  pub2_frame_2.timestamp_ns = ++t;
  pubrec_frame.timestamp_ns = ++t;

  req_map[1].push_back(pub1_frame_1);
  req_map[1].push_back(pub1_frame_2);
  req_map[1].push_back(pub2_frame_1);
  req_map[1].push_back(pub2_frame_2);

  resp_map[1].push_back(puback_frame);
  resp_map[1].push_back(pubrec_frame);

  // Update the state for PUBLISH packet ID 1 and QOS 1, packet ID 2 and QOS 2, packet ID 1 and QOS
  // 2
  state.send[std::tuple<uint32_t, uint32_t>(1, 1)] = 1;
  state.send[std::tuple<uint32_t, uint32_t>(1, 2)] = 1;

  result = StitchFrames(&req_map, &resp_map, &state);
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
}

TEST(MqttStitcherTest, DuplicateUnansweredRequests) {
  absl::flat_hash_map<packet_id_t, std::deque<Message>> req_map;
  absl::flat_hash_map<packet_id_t, std::deque<Message>> resp_map;

  RecordsWithErrorCount<Record> result;
  StateWrapper state;
  state.send = {};
  state.recv = {};

  Message pub1_frame_1, pub1_frame_2, pub2_frame_1, pub2_frame_2, puback_frame, pubrec_frame,
      subscribe_frame, suback_frame;
  pub1_frame_1 = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 1);
  pub1_frame_2 = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 1, 1);
  pub1_frame_2.dup = true;
  pub2_frame_1 = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 2, 2);
  pub2_frame_2 = CreateFrame(kRequest, MqttControlPacketType::PUBLISH, 2, 2);
  pub2_frame_2.dup = true;

  // Unanswered duplicate PUBLISH (QOS 1)
  int t = 0;
  pub1_frame_1.timestamp_ns = ++t;
  pub1_frame_2.timestamp_ns = ++t;
  pub2_frame_1.timestamp_ns = ++t;
  pub2_frame_2.timestamp_ns = ++t;

  req_map[1].push_back(pub1_frame_1);
  req_map[2].push_back(pub1_frame_2);
  req_map[2].push_back(pub2_frame_1);
  req_map[2].push_back(pub2_frame_2);

  state.send[std::tuple<uint32_t, uint32_t>(1, 1)] = 1;
  state.send[std::tuple<uint32_t, uint32_t>(2, 2)] = 1;

  result = StitchFrames(&req_map, &resp_map, &state);

  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 4);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);
}

}  // namespace mqtt
}  // namespace protocols
}  // namespace stirling
}  // namespace px
