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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>

#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/parse.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {

class MQTTParserTest : public ::testing::Test {};

TEST_F(MQTTParserTest, Properties) {
  Message frame;
  ParseState result_state;
  std::string_view frame_view;

  uint8_t payload_format_indicator_publish[] = {
      // header flags
      0x30,
      // message length
      0x20,
      // topic length
      0x00, 0x0a,
      // topic
      0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
      // properties length
      0x02,
      // payload format indicator code and value
      0x01, 0x01,
      // message
      0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x63, 0x72, 0x75, 0x65, 0x6c, 0x20, 0x77, 0x6f, 0x72,
      0x6c, 0x64};
  uint8_t message_expiry_interval_publish[] = {
      // header flags
      0x30,
      // message length
      0x23,
      // topic length
      0x00, 0x0a,
      // topic
      0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
      // properties length
      0x05,
      // message expiry interval code and value
      0x02, 0x03, 0xe8, 0x00, 0x00,
      // message
      0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x63, 0x72, 0x75, 0x65, 0x6c, 0x20, 0x77, 0x6f, 0x72,
      0x6c, 0x64};
  uint8_t content_type_publish[] = {// header flags
                                    0x30,
                                    // message length
                                    0x2b,
                                    // topic length
                                    0x00, 0x0a,
                                    // topic
                                    0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                    // properties length
                                    0x13,
                                    // content type code and value
                                    0x03, 0x00, 0x10, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61,
                                    0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x6a, 0x73, 0x6f, 0x6e,
                                    // message
                                    0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c,
                                    0x64};
  uint8_t response_topic_publish[] = {// header flags
                                      0x30,
                                      // message length
                                      0x21,
                                      // topic length
                                      0x00, 0x0a,
                                      // topic
                                      0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                      // properties length
                                      0x09,
                                      // response topic code, length and value
                                      0x08, 0x00, 0x06, 0x41, 0x42, 0x43, 0x58, 0x59, 0x5a,
                                      // message
                                      0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c,
                                      0x64};
  uint8_t correlation_data_publish[] = {// header flags
                                        0x30,
                                        // message length
                                        0x21,
                                        // topic length
                                        0x00, 0x0a,
                                        // topic
                                        0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                        // properties length
                                        0x09,
                                        // correlation data code, length and value
                                        0x09, 0x00, 0x06, 0x41, 0x42, 0x43, 0x58, 0x59, 0x5a,
                                        // message
                                        0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c,
                                        0x64};
  uint8_t subscription_id_subscribe[] = {// header flags
                                         0x82,
                                         // message length
                                         0x15,
                                         // message identifier
                                         0x00, 0x01,
                                         // properties length
                                         0x05,
                                         // subscription identifier code and value
                                         0x0b, 0x81, 0x84, 0x8b, 0x07,
                                         // topic length
                                         0x00, 0x0a,
                                         // topic
                                         0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                         // subscription options
                                         0x00};
  uint8_t session_exp_int_recv_max_connect[] = {// header flags
                                                0x10,
                                                // message length
                                                0x15,
                                                // protocol name length and protocol name
                                                0x00, 0x04, 0x4d, 0x51, 0x54, 0x54,
                                                // protocol version
                                                0x05,
                                                // connect flags
                                                0x02,
                                                // keep alive
                                                0x00, 0x3c,
                                                // properties length
                                                0x08,
                                                // session expiry interval code and value
                                                0x11, 0x00, 0x0f, 0x42, 0x40,
                                                // receive maximum code and value
                                                0x21, 0x00, 0x14,
                                                // client id length
                                                0x00, 0x00};
  uint8_t assigned_cid_topic_alias_max_recv_max_connack[] = {
      // header flags
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
      0x12, 0x00, 0x29, 0x61, 0x75, 0x74, 0x6f, 0x2d, 0x43, 0x38, 0x30, 0x36, 0x33, 0x38, 0x36,
      0x38, 0x2d, 0x37, 0x38, 0x30, 0x34, 0x2d, 0x33, 0x46, 0x36, 0x36, 0x2d, 0x30, 0x36, 0x42,
      0x38, 0x2d, 0x42, 0x41, 0x43, 0x44, 0x39, 0x46, 0x36, 0x37, 0x33, 0x43, 0x42, 0x30,
      // receive maximum code and value
      0x21, 0x00, 0x14};
  uint8_t auth_method_data_connect[] = {// header flags
                                        0x10,
                                        // message length
                                        0x37,
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
                                        0x2a,
                                        // authentication method code, length and value
                                        0x15, 0x00, 0x0d, 0x53, 0x43, 0x52, 0x41, 0x4d, 0x2d, 0x53,
                                        0x48, 0x41, 0x2d, 0x32, 0x35, 0x36,
                                        // authentication data code, length and value
                                        0x16, 0x00, 0x14, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x2d,
                                        0x66, 0x69, 0x72, 0x73, 0x74, 0x2d, 0x6d, 0x65, 0x73, 0x73,
                                        0x61, 0x67, 0x65,
                                        // receive maximum code and value
                                        0x21, 0x00, 0x14,
                                        // client id length
                                        0x00, 0x00};
  uint8_t req_prob_info_connect[] = {// header flags
                                     0x10,
                                     // message length
                                     0x12,
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
                                     0x05,
                                     // request problem information code and value
                                     0x17, 0x01,
                                     // receive maximum code and value
                                     0x21, 0x00, 0x14,
                                     // client id length
                                     0x00, 0x00};
  uint8_t will_delay_interval_connect[] = {// header flags
                                           0x10,
                                           // message length
                                           0x2b,
                                           // protocol name length
                                           0x00, 0x04,
                                           // protocol name
                                           0x4d, 0x51, 0x54, 0x54,
                                           // protocol version
                                           0x05,
                                           // connect flags
                                           0x06,
                                           // keep alive
                                           0x00, 0x3c,
                                           // properties length
                                           0x03,
                                           // receive maximum code and value
                                           0x21, 0x00, 0x14,
                                           // client id length
                                           0x00, 0x00,
                                           // will properties length
                                           0x05,
                                           // will delay interval code and value
                                           0x18, 0x00, 0x00, 0x00, 0x1e,
                                           // will topic length and value
                                           0x00, 0x0a, 0x77, 0x69, 0x6c, 0x6c, 0x2d, 0x74, 0x6f,
                                           0x70, 0x69, 0x63,
                                           // will message length and value
                                           0x00, 0x07, 0x67, 0x6f, 0x6f, 0x64, 0x62, 0x79, 0x65};
  uint8_t req_resp_info_connect[] = {// header flags
                                     0x10,
                                     // message length
                                     0x12,
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
                                     0x05,
                                     // request response information code and value
                                     0x19, 0x01,
                                     // receive maximum code and value
                                     0x21, 0x00, 0x14,
                                     // client id length
                                     0x00, 0x00};
  uint8_t topic_alias_publish[] = {// header flags
                                   0x30,
                                   // message length
                                   0x1b,
                                   // topic length
                                   0x00, 0x0a,
                                   // topic
                                   0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                   // properties length
                                   0x03,
                                   // topic alias code and value
                                   0x23, 0x00, 0x64,
                                   // message
                                   0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c,
                                   0x64};
  uint8_t user_prop_subscribe[] = {// header flags
                                   0x82,
                                   // message length
                                   0x2b,
                                   // message identifier
                                   0x00, 0x01,
                                   // properties length
                                   0x1b,
                                   // user property code
                                   0x26,
                                   // user property key length
                                   0x00, 0x0a,
                                   // user property key
                                   0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x6b, 0x65, 0x79,
                                   // user property value length
                                   0x00, 0x0c,
                                   // user property value
                                   0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x76, 0x61, 0x6c, 0x75,
                                   0x65,
                                   // topic length
                                   0x00, 0x0a,
                                   // topic
                                   0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                   // subscription options
                                   0x00};
  uint8_t max_packet_size_connect[] = {// header flags
                                       0x10,
                                       // message length
                                       0x15,
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
                                       0x08,
                                       // maximum packet size code and value
                                       0x27, 0x00, 0x10, 0x00, 0x00,
                                       // receive maximum code and value
                                       0x21, 0x00, 0x14,
                                       // client id length
                                       0x00, 0x00};

  frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(payload_format_indicator_publish));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["payload_format"], "utf-8");
  frame = Message();

  frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(message_expiry_interval_publish));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["message_expiry_interval"], "65536000");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(content_type_publish));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["content_type"], "application/json");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(response_topic_publish));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["response_topic"], "ABCXYZ");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(correlation_data_publish));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["correlation_data"], "ABCXYZ");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(subscription_id_subscribe));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["subscription_id"], "14860801");
  frame = Message();

  frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(session_exp_int_recv_max_connect));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["session_expiry_interval"], "1000000");
  EXPECT_EQ(frame.properties["receive_maximum"], "20");
  frame = Message();

  frame_view = CreateStringView<char>(
      CharArrayStringView<uint8_t>(assigned_cid_topic_alias_max_recv_max_connack));
  result_state = ParseFrame(message_type_t::kResponse, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["assigned_client_identifier"],
            "auto-C8063868-7804-3F66-06B8-BACD9F673CB0");
  EXPECT_EQ(frame.properties["topic_alias_maximum"], "10");
  EXPECT_EQ(frame.properties["receive_maximum"], "20");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(subscription_id_subscribe));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["subscription_id"], "14860801");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(auth_method_data_connect));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["auth_method"], "SCRAM-SHA-256");
  EXPECT_EQ(frame.properties["auth_data"], "client-first-message");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(req_prob_info_connect));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["request_problem_information"], "1");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(will_delay_interval_connect));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["will_delay_interval"], "30");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(req_resp_info_connect));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["request_response_information"], "1");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(topic_alias_publish));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["topic_alias"], "100");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(user_prop_subscribe));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["user-properties"], "{examplekey:examplevalue}");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(max_packet_size_connect));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.properties["maximum_packet_size"], "1048576");
  frame = Message();
}

TEST_F(MQTTParserTest, Payload) {
  Message frame;
  ParseState result_state;
  std::string_view frame_view;

  uint8_t kConnectFrame[] = {// header flags
                             0x10,
                             // message length
                             0x41,
                             // protocol name length
                             0x00, 0x04,
                             // protocol name
                             0x4d, 0x51, 0x54, 0x54,
                             // protocol version
                             0x05,
                             // connect flags
                             0xc6,
                             // keep alive
                             0x00, 0x3c,
                             // properties length
                             0x03,
                             // receive maximum code and value
                             0x21, 0x00, 0x14,
                             // client id length
                             0x00, 0x00,
                             // will properties length
                             0x05,
                             // will delay interval code and value
                             0x18, 0x00, 0x00, 0x00, 0x1e,
                             // will topic length and value
                             0x00, 0x0a, 0x77, 0x69, 0x6c, 0x6c, 0x2d, 0x74, 0x6f, 0x70, 0x69, 0x63,
                             // will message length and value
                             0x00, 0x07, 0x67, 0x6f, 0x6f, 0x64, 0x62, 0x79, 0x65,
                             // username length and value
                             0x00, 0x09, 0x64, 0x75, 0x6d, 0x6d, 0x79, 0x75, 0x73, 0x65, 0x72,
                             // password length and value
                             0x00, 0x09, 0x64, 0x75, 0x6d, 0x6d, 0x79, 0x70, 0x61, 0x73, 0x73};
  uint8_t kPublishFrame[] = {// header flags
                             0x32,
                             // message length
                             0x1a,
                             // topic length
                             0x00, 0x0a, 0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                             // message identifier
                             0x00, 0x01,
                             // properties length
                             0x00,
                             // message
                             0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};
  uint8_t kSubscribeFrame[] = {// header fields
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
  uint8_t kSubackFrame[] = {// header flags
                            0x90,
                            // message length
                            0x04,
                            // message identifier
                            0x00, 0x01,
                            // properties length
                            0x00,
                            // reason code
                            0x00};
  uint8_t kUnsubscribeFrame[] = {// header flags
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
  uint8_t kUnsubackFrame[] = {// header flags
                              0xb0,
                              // message length
                              0x04,
                              // message identifier
                              0x00, 0x02,
                              // properties length
                              0x00,
                              // reason code
                              0x00};

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnectFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.payload["will_topic"], "will-topic");
  EXPECT_EQ(frame.payload["will_payload"], "goodbye");
  EXPECT_EQ(frame.payload["username"], "dummyuser");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.payload["publish_message"], "hello world");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubscribeFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.payload["topic_filter"], "test/topic");
  std::map<std::string, uint8_t> subscription_opts(
      {{"maximum_qos", 0}, {"no_local", 0}, {"retain_as_published", 0}, {"retain_handling", 0}});
  EXPECT_EQ(frame.payload["subscription_options"], ToJSONString(subscription_opts));
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubackFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.payload["reason_code"], "0");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubscribeFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.payload["topic_filter"], "test/topic");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubackFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.payload["reason_code"], "0");
  frame = Message();
}

TEST_F(MQTTParserTest, Headers) {
  Message frame;
  std::string_view frame_view;
  ParseState result_state;

  uint8_t kConnectFrame[] = {// header flags
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
  uint8_t kConnackFrame[] = {// header flags
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
                             0x12, 0x00, 0x29, 0x61, 0x75, 0x74, 0x6f, 0x2d, 0x43, 0x38, 0x30, 0x36,
                             0x33, 0x38, 0x36, 0x38, 0x2d, 0x37, 0x38, 0x30, 0x34, 0x2d, 0x33, 0x46,
                             0x36, 0x36, 0x2d, 0x30, 0x36, 0x42, 0x38, 0x2d, 0x42, 0x41, 0x43, 0x44,
                             0x39, 0x46, 0x36, 0x37, 0x33, 0x43, 0x42, 0x30,
                             // receive maximum code and value
                             0x21, 0x00, 0x14};
  uint8_t kPublishFrame[] = {// header flags
                             0x32,
                             // message length
                             0x1a,
                             // topic length
                             0x00, 0x0a, 0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                             // message identifier
                             0x00, 0x01,
                             // properties length
                             0x00,
                             // message
                             0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};
  uint8_t kPubackFrame[] = {// header flags
                            0x40,
                            // message length
                            0x03,
                            // message identifier
                            0x00, 0x01, 0x10};
  uint8_t kPubrecFrame[] = {// header flags
                            0x50,
                            // message length
                            0x02,
                            // message identifier
                            0x00, 0x01};
  uint8_t kPubrelFrame[] = {// header flags
                            0x62,
                            // message length
                            0x02,
                            // message identifier
                            0x00, 0x01};
  uint8_t kPubcompFrame[] = {// header flags
                             0x70,
                             // message length
                             0x02,
                             // message identifier
                             0x00, 0x01};
  uint8_t kSubscribeFrame[] = {// header fields
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
  uint8_t kSubackFrame[] = {// header flags
                            0x90,
                            // message length
                            0x04,
                            // message identifier
                            0x00, 0x01,
                            // properties length
                            0x00,
                            // reason code
                            0x00};
  uint8_t kUnsubscribeFrame[] = {// header flags
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
  uint8_t kUnsubackFrame[] = {// header flags
                              0xb0,
                              // message length
                              0x04,
                              // message identifier
                              0x00, 0x02,
                              // properties length
                              0x00,
                              // reason code
                              0x00};
  uint8_t kPingreqFrame[] = {// header flags
                             0xc0,
                             // message length
                             0x00};
  uint8_t kPingrespFrame[] = {// header flags
                              0xd0,
                              // message length
                              0x00};
  uint8_t kDisconnectFrame[] = {// header flags
                                0xe0,
                                // message length
                                0x01,
                                // reason code
                                0x04};
  uint8_t kAuthFrame_success[] = {// header flags
                                  0xf0,
                                  // message length
                                  0x00};
  uint8_t kAuthFrame_cont_auth[] = {// header flags
                                    0xf0,
                                    // message length
                                    0x01,
                                    // reason code
                                    0x18};

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnectFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 1);
  EXPECT_EQ(frame.header_fields["remaining_length"], 16);
  EXPECT_EQ(frame.header_fields["username_flag"], 0);
  EXPECT_EQ(frame.header_fields["password_flag"], 0);
  EXPECT_EQ(frame.header_fields["will_retain"], 0);
  EXPECT_EQ(frame.header_fields["will_qos"], 0);
  EXPECT_EQ(frame.header_fields["will_flag"], 0);
  EXPECT_EQ(frame.header_fields["clean_start"], 1);
  EXPECT_EQ(frame.header_fields["keep_alive"], 60);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnackFrame));
  result_state = ParseFrame(message_type_t::kResponse, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 2);
  EXPECT_EQ(frame.header_fields["remaining_length"], 53);
  EXPECT_EQ(frame.header_fields["session_present"], 0);
  EXPECT_EQ(frame.header_fields["reason_code"], 0);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 3);
  EXPECT_EQ(frame.header_fields["remaining_length"], 26);
  EXPECT_EQ(frame.dup, false);
  EXPECT_EQ(frame.retain, false);
  EXPECT_EQ(frame.header_fields["qos"], 1);
  EXPECT_EQ(frame.payload["topic_name"], "test/topic");
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubackFrame));
  result_state = ParseFrame(message_type_t::kResponse, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 4);
  EXPECT_EQ(frame.header_fields["remaining_length"], 3);
  EXPECT_EQ(frame.header_fields["packet_identifier"], 1);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrecFrame));
  result_state = ParseFrame(message_type_t::kResponse, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 5);
  EXPECT_EQ(frame.header_fields["remaining_length"], 2);
  EXPECT_EQ(frame.header_fields["packet_identifier"], 1);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrelFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 6);
  EXPECT_EQ(frame.header_fields["remaining_length"], 2);
  EXPECT_EQ(frame.header_fields["packet_identifier"], 1);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubcompFrame));
  result_state = ParseFrame(message_type_t::kResponse, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 7);
  EXPECT_EQ(frame.header_fields["remaining_length"], 2);
  EXPECT_EQ(frame.header_fields["packet_identifier"], 1);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubscribeFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 8);
  EXPECT_EQ(frame.header_fields["remaining_length"], 16);
  EXPECT_EQ(frame.header_fields["packet_identifier"], 1);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubackFrame));
  result_state = ParseFrame(message_type_t::kResponse, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 9);
  EXPECT_EQ(frame.header_fields["remaining_length"], 4);
  EXPECT_EQ(frame.payload["reason_code"], "0");
  EXPECT_EQ(frame.header_fields["packet_identifier"], 1);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubscribeFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 10);
  EXPECT_EQ(frame.header_fields["remaining_length"], 15);
  EXPECT_EQ(frame.header_fields["packet_identifier"], 2);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubackFrame));
  result_state = ParseFrame(message_type_t::kResponse, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 11);
  EXPECT_EQ(frame.header_fields["remaining_length"], 4);
  EXPECT_EQ(frame.header_fields["packet_identifier"], 2);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingreqFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 12);
  EXPECT_EQ(frame.header_fields["remaining_length"], 0);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingrespFrame));
  result_state = ParseFrame(message_type_t::kResponse, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 13);
  EXPECT_EQ(frame.header_fields["remaining_length"], 0);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kDisconnectFrame));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 14);
  EXPECT_EQ(frame.header_fields["remaining_length"], 1);
  EXPECT_EQ(frame.header_fields["reason_code"], 4);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kAuthFrame_success));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 15);
  EXPECT_EQ(frame.header_fields["reason_code"], 0);
  frame = Message();

  frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kAuthFrame_cont_auth));
  result_state = ParseFrame(message_type_t::kRequest, &frame_view, &frame);
  ASSERT_EQ(result_state, ParseState::kSuccess);
  EXPECT_EQ(frame.control_packet_type, 15);
  EXPECT_EQ(frame.header_fields["reason_code"], 0x18);
}

}  // namespace mqtt
}  // namespace protocols
}  // namespace stirling
}  // namespace px
