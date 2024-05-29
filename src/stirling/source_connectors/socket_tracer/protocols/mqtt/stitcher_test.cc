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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/stitcher.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {

constexpr uint8_t kConnect[] = {  // header flags
    0x10,
    // message length
    0x10,
    // protocol name length
    0x00, 0x04,
    // protocol name
    0x4d, 0x51, 0x54, 0x54,
    // protocol version
    0x05,
    // connect flags
    0x02,
    // keep alive
    0x00, 0x3c,
    // properties length
    0x03,
    // receive maximum code and value
    0x21, 0x00, 0x14,
    // client id length
    0x00, 0x00};

constexpr uint8_t kConnack[] = {  // header flags
    0x20,
    // message length
    0x35,
    // acknowledge flags
    0x00,
    // reason code
    0x00,
    // properties length
    0x32,
    // topic alias maximum code and value
    0x22, 0x00, 0x0a,
    // assigned client identifier code, length and value
    0x12, 0x00, 0x29, 0x61, 0x75, 0x74, 0x6f, 0x2d, 0x43, 0x38, 0x30, 0x36, 0x33, 0x38, 0x36, 0x38,
    0x2d, 0x37, 0x38, 0x30, 0x34, 0x2d, 0x33, 0x46, 0x36, 0x36, 0x2d, 0x30, 0x36, 0x42, 0x38, 0x2d,
    0x42, 0x41, 0x43, 0x44, 0x39, 0x46, 0x36, 0x37, 0x33, 0x43, 0x42, 0x30,
    // receive maximum code and value
    0x21, 0x00, 0x14};

// Publish with QOS 0
constexpr uint8_t kPublishQosZero[] = {  // header flags
    0x30,
    // message length
    0x18,
    // topic length
    0x00, 0x0a,
    // topic
    0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
    // properties
    0x00,
    // message
    0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};

// Publish with QOS 1 and PID 1
constexpr uint8_t kPublishQosOne[] = {  // header flags
    0x32,
    // message length
    0x1a,
    // topic length
    0x00, 0x0a,
    // topic,
    0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
    // packet identifier
    0x00, 0x01,
    // properties
    0x00,
    // message
    0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};

constexpr uint8_t kPublishQosTwo[] = {  // header flags
    0x34,
    // message length
    0x1a,
    // topic length
    0x00, 0x0a,
    // topic
    0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
    // packet identifier
    0x00, 0x01,
    // properties
    0x00,
    // message
    0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};

// Puback with packet ID 1
constexpr uint8_t kPubackPidOne[] = {  // header flags
    0x40,
    // message length
    0x03,
    // message identifier
    0x00, 0x01, 0x10};

// Pubrec with packet ID 1
constexpr uint8_t kPubrec[] = {  // header flags
    0x50,
    // message length
    0x02,
    // message identifier
    0x00, 0x01};

// Pubrel with packet ID 1
constexpr uint8_t kPubrel[] = {  // header flags
    0x62,
    // message length
    0x02,
    // message identifier
    0x00, 0x01};

// Pubcomp with packet ID 1
constexpr uint8_t kPubcomp[] = {  // header flags
    0x70,
    // message length
    0x02,
    // message identifier
    0x00, 0x01};

constexpr uint8_t kSubscribeFrame[] = {  // header fields
    0x82,
    // message length
    0x10,
    // message identifier
    0x00, 0x01,
    // properties length
    0x00,
    // topic length
    0x00, 0x0a,
    // topic
    0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
    // subscription options
    0x00};
constexpr uint8_t kSubackFrame[] = {  // header flags
    0x90,
    // message length
    0x04,
    // message identifier
    0x00, 0x01,
    // properties length
    0x00,
    // reason code
    0x00};
constexpr uint8_t kUnsubscribeFrame[] = {  // header flags
    0xa2,
    // message length
    0x0f,
    // message identifier
    0x00, 0x02,
    // properties length
    0x00,
    // topic length
    0x00, 0x0a,
    // topic
    0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63};
constexpr uint8_t kUnsubackFrame[] = {  // header flags
    0xb0,
    // message length
    0x04,
    // message identifier
    0x00, 0x02,
    // properties length
    0x00,
    // reason code
    0x00};
constexpr uint8_t kPingreqFrame[] = {  // header flags
    0xc0,
    // message length
    0x00};
constexpr uint8_t kPingrespFrame[] = {  // header flags
    0xd0,
    // message length
    0x00};
constexpr uint8_t kDisconnectFrame[] = {  // header flags
    0xe0,
    // message length
    0x01,
    // reason code
    0x04};
constexpr uint8_t kAuthFrame_success[] = {  // header flags
    0xf0,
    // message length
    0x00};

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

  std::string_view frame_view;
  RecordsWithErrorCount<Record> result;

  Message connect_frame, pingreq_frame;
  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnect));
  ParseFrame(message_type_t::kRequest, &frame_view, &connect_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingreqFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &pingreq_frame);

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

  std::string_view frame_view;
  RecordsWithErrorCount<Record> result;

  Message connack_frame, pingresp_frame;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnack));
  ParseFrame(message_type_t::kResponse, &frame_view, &connack_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingrespFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &pingresp_frame);

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

  std::string_view frame_view;
  RecordsWithErrorCount<Record> result;

  Message connect_frame, connack_frame, pingreq_frame, pingresp_frame, pub1_frame, pub1_pid3_frame,
      puback_frame, puback_pid3_frame, pub2_frame, pub2_pid2_frame, pubrec_frame, pubrec_pid2_frame,
      pubrel_frame, pubrel_pid2_frame, pubcomp_frame, pubcomp_pid2_frame, sub_frame, suback_frame,
      unsub_frame, unsuback_frame;
  // The order in terms of timestamps is Connect, Connack, Pingreq and Pingresp
  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnect));
  ParseFrame(message_type_t::kRequest, &frame_view, &connect_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnack));
  ParseFrame(message_type_t::kResponse, &frame_view, &connack_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingreqFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &pingreq_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingrespFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &pingresp_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosOne));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub1_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosOne));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub1_pid3_frame);
  pub1_pid3_frame.header_fields["message_identifier"] = 3;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosTwo));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub2_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosTwo));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub2_pid2_frame);
  pub2_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubackPidOne));
  ParseFrame(message_type_t::kResponse, &frame_view, &puback_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubackPidOne));
  ParseFrame(message_type_t::kResponse, &frame_view, &puback_pid3_frame);
  puback_pid3_frame.header_fields["message_identifier"] = 3;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrec));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubrec_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrec));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubrec_pid2_frame);
  pubrec_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrel));
  ParseFrame(message_type_t::kRequest, &frame_view, &pubrel_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrel));
  ParseFrame(message_type_t::kRequest, &frame_view, &pubrel_pid2_frame);
  pubrel_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubcomp));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubcomp_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubcomp));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubcomp_pid2_frame);
  pubcomp_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubscribeFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &sub_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubackFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &suback_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubscribeFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &unsub_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubackFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &unsuback_frame);

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

  std::string_view frame_view;
  RecordsWithErrorCount<Record> result;

  Message connect_frame, connack_frame, pingresp_frame;
  // The order in terms of timestamps is Connect, Connack, Pingreq and Pingresp
  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnect));
  ParseFrame(message_type_t::kRequest, &frame_view, &connect_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnack));
  ParseFrame(message_type_t::kResponse, &frame_view, &connack_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingrespFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &pingresp_frame);

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

  std::string_view frame_view;
  RecordsWithErrorCount<Record> result;

  Message connect_frame, connack_frame, pingreq_frame, pingresp_frame, pub1_frame, pub1_pid3_frame,
      puback_frame, puback_pid3_frame, pub2_frame, pub2_pid2_frame, pubrec_frame, pubrec_pid2_frame,
      pubrel_frame, pubrel_pid2_frame, pubcomp_frame, pubcomp_pid2_frame, sub_frame, suback_frame,
      unsub_frame, unsuback_frame;
  // The order in terms of timestamps is Connect, Connack, Pingreq and Pingresp
  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnect));
  ParseFrame(message_type_t::kRequest, &frame_view, &connect_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnack));
  ParseFrame(message_type_t::kResponse, &frame_view, &connack_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingreqFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &pingreq_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingrespFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &pingresp_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosOne));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub1_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosOne));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub1_pid3_frame);
  pub1_pid3_frame.header_fields["message_identifier"] = 3;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosTwo));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub2_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosTwo));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub2_pid2_frame);
  pub2_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubackPidOne));
  ParseFrame(message_type_t::kResponse, &frame_view, &puback_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubackPidOne));
  ParseFrame(message_type_t::kResponse, &frame_view, &puback_pid3_frame);
  puback_pid3_frame.header_fields["message_identifier"] = 3;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrec));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubrec_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrec));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubrec_pid2_frame);
  pubrec_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrel));
  ParseFrame(message_type_t::kRequest, &frame_view, &pubrel_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrel));
  ParseFrame(message_type_t::kRequest, &frame_view, &pubrel_pid2_frame);
  pubrel_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubcomp));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubcomp_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubcomp));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubcomp_pid2_frame);
  pubcomp_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubscribeFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &sub_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubackFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &suback_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubscribeFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &unsub_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubackFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &unsuback_frame);

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

  std::string_view frame_view;
  RecordsWithErrorCount<Record> result;

  Message connect_frame, connack_frame, pingreq_frame, pingresp_frame, pub1_frame, pub1_pid3_frame,
      puback_frame, puback_pid3_frame, pub2_frame, pub2_pid2_frame, pubrec_frame, pubrec_pid2_frame,
      pubrel_frame, pubrel_pid2_frame, pubcomp_frame, pubcomp_pid2_frame, sub_frame, suback_frame,
      unsub_frame, unsuback_frame;
  // The order in terms of timestamps is Connect, Connack, Pingreq and Pingresp
  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnect));
  ParseFrame(message_type_t::kRequest, &frame_view, &connect_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnack));
  ParseFrame(message_type_t::kResponse, &frame_view, &connack_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingreqFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &pingreq_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingrespFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &pingresp_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosOne));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub1_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosOne));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub1_pid3_frame);
  pub1_pid3_frame.header_fields["message_identifier"] = 3;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosTwo));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub2_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosTwo));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub2_pid2_frame);
  pub2_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubackPidOne));
  ParseFrame(message_type_t::kResponse, &frame_view, &puback_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubackPidOne));
  ParseFrame(message_type_t::kResponse, &frame_view, &puback_pid3_frame);
  puback_pid3_frame.header_fields["message_identifier"] = 3;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrec));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubrec_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrec));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubrec_pid2_frame);
  pubrec_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrel));
  ParseFrame(message_type_t::kRequest, &frame_view, &pubrel_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrel));
  ParseFrame(message_type_t::kRequest, &frame_view, &pubrel_pid2_frame);
  pubrel_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubcomp));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubcomp_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubcomp));
  ParseFrame(message_type_t::kResponse, &frame_view, &pubcomp_pid2_frame);
  pubcomp_pid2_frame.header_fields["message_identifier"] = 2;

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubscribeFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &sub_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubackFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &suback_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubscribeFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &unsub_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubackFrame));
  ParseFrame(message_type_t::kResponse, &frame_view, &unsuback_frame);

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

  std::string_view frame_view;
  RecordsWithErrorCount<Record> result;

  Message pub0_frame, disconnect_frame, auth_frame, connect_frame, connack_frame;
  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishQosZero));
  ParseFrame(message_type_t::kRequest, &frame_view, &pub0_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kDisconnectFrame));
  ParseFrame(message_type_t::kRequest, &frame_view, &disconnect_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kAuthFrame_success));
  ParseFrame(message_type_t::kRequest, &frame_view, &auth_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnect));
  ParseFrame(message_type_t::kRequest, &frame_view, &connect_frame);

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnack));
  ParseFrame(message_type_t::kResponse, &frame_view, &connack_frame);

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

}  // namespace mqtt
}  // namespace protocols
}  // namespace stirling
}  // namespace px
