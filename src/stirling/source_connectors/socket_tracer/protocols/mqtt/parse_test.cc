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

#include "src/stirling/utils/binary_decoder.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/parse.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {

class MQTTParserTest : public ::testing::Test {};

TEST_F(MQTTParserTest, Properties) {
    std::string_view frame_view;
    ParseResult parse_result;
    std::deque<Message> frames;

    uint8_t payload_format_indicator_publish[] = {0x30, 0x20, 0x00, 0x0a, 0x74, 0x65, 0x73,
                                                  0x74, 0x2f, 0x74, 0x6f, 0x70,0x69,
                                                  0x63, 0x02, 0x01, 0x01, 0x68, 0x65,
                                                  0x6c, 0x6c, 0x6f, 0x20, 0x63,0x72,
                                                  0x75, 0x65, 0x6c, 0x20, 0x77, 0x6f,
                                                  0x72, 0x6c, 0x64};
    uint8_t message_expiry_interval_publish[] = {0x30, 0x23, 0x00, 0x0a, 0x74, 0x65, 0x73,
                                                 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69,
                                                 0x63, 0x05, 0x02, 0x03, 0xe8, 0x00,
                                                 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f,
                                                 0x20, 0x63, 0x72, 0x75, 0x65, 0x6c,
                                                 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};
    uint8_t content_type_publish[] = {0x30, 0x2b, 0x00, 0x0a, 0x74, 0x65, 0x73,
                                      0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                      0x13, 0x03, 0x00, 0x10, 0x61, 0x70, 0x70,
                                      0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f,
                                      0x6e, 0x2f, 0x6a, 0x73, 0x6f, 0x6e, 0x68,
                                      0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f,
                                      0x72, 0x6c, 0x64};
    uint8_t response_topic_publish[] = {0x30, 0x21, 0x00, 0x0a, 0x74, 0x65, 0x73,
                                        0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                        0x09, 0x08, 0x00, 0x06, 0x41, 0x42, 0x43,
                                        0x58, 0x59, 0x5a, 0x68, 0x65, 0x6c, 0x6c,
                                        0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};
    uint8_t correlation_data_publish[] = {0x30, 0x21, 0x00, 0x0a, 0x74, 0x65, 0x73,
                                          0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63,
                                          0x09, 0x09, 0x00, 0x06, 0x41, 0x42, 0x43,
                                          0x58, 0x59, 0x5a, 0x68, 0x65, 0x6c, 0x6c,
                                          0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};
    uint8_t subscription_id_subscribe[] = {0x82, 0x15, 0x00, 0x01, 0x05, 0x0b, 0x81,
                                           0x84, 0x8b, 0x07, 0x00, 0x0a, 0x74, 0x65,
                                           0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69,
                                           0x63, 0x00};
    uint8_t session_exp_int_recv_max_connect[] = {0x10, 0x15, 0x00, 0x04, 0x4d, 0x51, 0x54,
                                                  0x54, 0x05, 0x02, 0x00, 0x3c, 0x08, 0x11,
                                                  0x00, 0x0f, 0x42, 0x40, 0x21, 0x00, 0x14,
                                                  0x00, 0x00};
    uint8_t assigned_cid_topic_alias_max_recv_max_connack[] = {0x20, 0x35, 0x00, 0x00, 0x32, 0x22, 0x00,
                                                               0x0a, 0x12, 0x00, 0x29, 0x61, 0x75, 0x74,
                                                               0x6f, 0x2d, 0x43, 0x38, 0x30, 0x36, 0x33,
                                                               0x38, 0x36, 0x38, 0x2d, 0x37, 0x38, 0x30,
                                                               0x34, 0x2d, 0x33, 0x46, 0x36, 0x36, 0x2d,
                                                               0x30, 0x36, 0x42, 0x38, 0x2d, 0x42, 0x41,
                                                               0x43, 0x44, 0x39, 0x46, 0x36, 0x37, 0x33,
                                                               0x43, 0x42, 0x30, 0x21, 0x00, 0x14};
    uint8_t auth_method_data_connect[] = {0x10, 0x37, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54, 0x05,
                                          0x02, 0x00, 0x3c, 0x2a, 0x15, 0x00, 0x0d, 0x53,
                                          0x43, 0x52, 0x41, 0x4d, 0x2d, 0x53, 0x48, 0x41,
                                          0x2d, 0x32, 0x35, 0x36, 0x16, 0x00, 0x14, 0x63,
                                          0x6c, 0x69, 0x65, 0x6e, 0x74, 0x2d, 0x66, 0x69,
                                          0x72, 0x73, 0x74, 0x2d, 0x6d, 0x65, 0x73, 0x73,
                                          0x61, 0x67, 0x65, 0x21, 0x00, 0x14, 0x00, 0x00};
    uint8_t req_prob_info_connect[] = {0x10, 0x12, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54, 0x05,
                               0x02, 0x00, 0x3c, 0x05, 0x17, 0x01, 0x21, 0x00,
                               0x14, 0x00, 0x00};
    uint8_t will_delay_interval_connect[] = {0x10, 0x2b, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54, 0x05,
                                             0x06, 0x00, 0x3c, 0x03, 0x21, 0x00, 0x14, 0x00,
                                             0x00, 0x05, 0x18, 0x00, 0x00, 0x00, 0x1e, 0x00,
                                             0x0a, 0x77, 0x69, 0x6c, 0x6c, 0x2d, 0x74, 0x6f,
                                             0x70, 0x69, 0x63, 0x00, 0x07, 0x67, 0x6f, 0x6f,
                                             0x64, 0x62, 0x79, 0x65};
    uint8_t req_resp_info_connect[] = {0x10, 0x12, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54,
                                       0x05, 0x02, 0x00, 0x3c, 0x05, 0x19, 0x01,
                                       0x21, 0x00, 0x14, 0x00, 0x00};
    uint8_t topic_alias_publish[] = {0x30, 0x1b, 0x00, 0x0a, 0x74, 0x65, 0x73, 0x74,
                                     0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63, 0x03, 0x23,
                                     0x00, 0x64, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20,
                                     0x77, 0x6f, 0x72, 0x6c, 0x64};
    uint8_t user_prop_subscribe[] = {0x82, 0x2b, 0x00, 0x01, 0x1b, 0x26, 0x00, 0x0a,
                                     0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x6b,
                                     0x65, 0x79, 0x00, 0x0c, 0x65, 0x78, 0x61, 0x6d,
                                     0x70, 0x6c, 0x65, 0x76, 0x61, 0x6c, 0x75, 0x65,
                                     0x00, 0x0a, 0x74, 0x65, 0x73, 0x74, 0x2f, 0x74,
                                     0x6f, 0x70, 0x69, 0x63, 0x00};
    uint8_t max_packet_size_connect[] = {0x10, 0x15, 0x00, 0x04, 0x4d, 0x51, 0x54,
                                         0x54, 0x05, 0x02, 0x00, 0x3c, 0x08, 0x27,
                                         0x00, 0x10, 0x00, 0x00, 0x21, 0x00, 0x14,
                                         0x00, 0x00};

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(payload_format_indicator_publish));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["payload_format"], "utf-8");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(message_expiry_interval_publish));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["message_expiry_interval"], "65536000");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(content_type_publish));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["content_type"], "application/json");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(response_topic_publish));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["response_topic"], "ABCXYZ");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(correlation_data_publish));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["correlation_data"], "ABCXYZ");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(subscription_id_subscribe));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["subscription_id"], "14860801");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(session_exp_int_recv_max_connect));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["session_expiry_interval"], "1000000");
    EXPECT_EQ(frames[0].properties["receive_maximum"], "20");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(assigned_cid_topic_alias_max_recv_max_connack));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["assigned_client_identifier"], "auto-C8063868-7804-3F66-06B8-BACD9F673CB0");
    EXPECT_EQ(frames[0].properties["topic_alias_maximum"], "10");
    EXPECT_EQ(frames[0].properties["receive_maximum"], "20");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(subscription_id_subscribe));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["subscription_id"], "14860801");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(auth_method_data_connect));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["auth_method"], "SCRAM-SHA-256");
    EXPECT_EQ(frames[0].properties["auth_data"], "client-first-message");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(req_prob_info_connect));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["request_problem_information"], "1");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(will_delay_interval_connect));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["will_delay_interval"], "30");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(req_resp_info_connect));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["request_response_information"], "1");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(topic_alias_publish));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["topic_alias"], "100");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(user_prop_subscribe));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["user-properties"], "{examplekey:examplevalue}");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(max_packet_size_connect));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].properties["maximum_packet_size"], "1048576");
    frames.clear();

    // TODO: Server Keep Alive in CONNACK
    // TODO: Response Information in CONNACK
    // TODO: Max QOS in CONNACK
    // TODO: Retain Available in CONNACK
    // TODO: Wildcard Subscription Available in CONNACK
    // TODO: Subscription Identifier Available in CONNACK
    // TODO: Shared Subscription Available in CONNACK
}

TEST_F(MQTTParserTest, Payload) {
    std::deque<Message> frames;
    std::string_view frame_view;
    ParseResult parse_result;

    uint8_t kConnectFrame[] = {0x10, 0x41, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54,
                               0x05, 0xc6, 0x00, 0x3c, 0x03, 0x21, 0x00, 0x14,
                               0x00, 0x00, 0x05, 0x18, 0x00, 0x00, 0x00, 0x1e,
                               0x00, 0x0a, 0x77, 0x69, 0x6c, 0x6c, 0x2d, 0x74,
                               0x6f, 0x70, 0x69, 0x63, 0x00, 0x07, 0x67, 0x6f,
                               0x6f, 0x64, 0x62, 0x79, 0x65, 0x00, 0x09, 0x64,
                               0x75, 0x6d, 0x6d, 0x79, 0x75, 0x73, 0x65, 0x72,
                               0x00, 0x09, 0x64, 0x75, 0x6d, 0x6d, 0x79, 0x70,
                               0x61, 0x73, 0x73};
    uint8_t kPublishFrame[] = {0x32, 0x1a, 0x00, 0x0a, 0x74, 0x65, 0x73, 0x74,
                               0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63, 0x00, 0x01,
                               0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77,
                               0x6f, 0x72, 0x6c, 0x64};
    uint8_t kSubscribeFrame[] = {0x82, 0x10, 0x00, 0x01, 0x00, 0x00, 0x0a, 0x74,
                                 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63, 0x00};
    uint8_t kSubackFrame[] = {0x90, 0x04, 0x00, 0x01, 0x00, 0x00};
    uint8_t kUnsubscribeFrame[] = {0xa2, 0x0f, 0x00, 0x02, 0x00, 0x00, 0x0a,
                                   0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f,
                                   0x70, 0x69, 0x63};
    uint8_t kUnsubackFrame[] = {0xb0, 0x04, 0x00, 0x02, 0x00, 0x00};

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnectFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].payload["will_topic"], "will-topic");
    EXPECT_EQ(frames[0].payload["will_payload"], "goodbye");
    EXPECT_EQ(frames[0].payload["username"], "dummyuser");
    EXPECT_EQ(frames[0].payload["password"], "dummypass");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].payload["publish_message"], "hello world");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubscribeFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].payload["topic_filter"], "test/topic");
    EXPECT_EQ(frames[0].payload["subscription_options"], "{maximum_qos : 0, no_local : 0, retain_as_published : 0, retain_handling : 0}");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubackFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].payload["reason_code"], "0");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubscribeFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].payload["topic_filter"], "test/topic");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubackFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].payload["reason_code"], "0");
    frames.clear();
}

TEST_F(MQTTParserTest, Headers) {
    std::deque<Message> frames;
    std::string_view frame_view;
    ParseResult parse_result;

    uint8_t kConnectFrame[] = {0x10, 0x10, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54,
                               0x05, 0x02, 0x00, 0x3c, 0x03, 0x21, 0x00, 0x14,
                               0x00, 0x00};
    uint8_t kConnackFrame[] = {0x20, 0x35, 0x00, 0x00, 0x32, 0x22, 0x00,
                                0x0a, 0x12, 0x00, 0x29, 0x61, 0x75, 0x74,
                                0x6f, 0x2d, 0x43, 0x38, 0x30, 0x36, 0x33,
                                0x38, 0x36, 0x38, 0x2d, 0x37, 0x38, 0x30,
                                0x34, 0x2d, 0x33, 0x46, 0x36, 0x36, 0x2d,
                                0x30, 0x36, 0x42, 0x38, 0x2d, 0x42, 0x41,
                                0x43, 0x44, 0x39, 0x46, 0x36, 0x37, 0x33,
                                0x43, 0x42, 0x30, 0x21, 0x00, 0x14};
    uint8_t kPublishFrame[] = {0x32, 0x1a, 0x00, 0x0a, 0x74, 0x65, 0x73, 0x74,
                               0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63, 0x00, 0x01,
                               0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77,
                               0x6f, 0x72, 0x6c, 0x64};
    uint8_t kPubackFrame[] = {0x40, 0x03, 0x00, 0x01, 0x10};
    uint8_t kPubrecFrame[] = {0x50, 0x02, 0x00, 0x01};
    uint8_t kPubrelFrame[] = {0x62, 0x02, 0x00, 0x01};
    uint8_t kPubcompFrame[] = {0x70, 0x02, 0x00, 0x01};
    uint8_t kSubscribeFrame[] = {0x82, 0x10, 0x00, 0x01, 0x00, 0x00, 0x0a, 0x74,
                                 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f, 0x70, 0x69, 0x63, 0x00};
    uint8_t kSubackFrame[] = {0x90, 0x04, 0x00, 0x01, 0x00, 0x00};
    uint8_t kUnsubscribeFrame[] = {0xa2, 0x0f, 0x00, 0x02, 0x00, 0x00, 0x0a,
                                   0x74, 0x65, 0x73, 0x74, 0x2f, 0x74, 0x6f,
                                   0x70, 0x69, 0x63};
    uint8_t kUnsubackFrame[] = {0xb0, 0x04, 0x00, 0x02, 0x00, 0x00};
    uint8_t kPingreqFrame[] = {0xc0, 0x00};
    uint8_t kPingrespFrame[] = {0xd0, 0x00};
    uint8_t kDisconnectFrame[] = {0xe0, 0x01, 0x04};

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnectFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "CONNECT");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)16);
    EXPECT_EQ(frames[0].header_fields["username_flag"], (unsigned long)0);
    EXPECT_EQ(frames[0].header_fields["password_flag"], (unsigned long)0);
    EXPECT_EQ(frames[0].header_fields["will_retain"], (unsigned long)0);
    EXPECT_EQ(frames[0].header_fields["will_qos"], (unsigned long)0);
    EXPECT_EQ(frames[0].header_fields["will_flag"], (unsigned long)0);
    EXPECT_EQ(frames[0].header_fields["clean_start"], (unsigned long)1);
    EXPECT_EQ(frames[0].header_fields["keep_alive"], (unsigned long)60);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kConnackFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "CONNACK");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)53);
    EXPECT_EQ(frames[0].header_fields["session_present"], (unsigned long)0);
    EXPECT_EQ(frames[0].header_fields["reason_code"], (unsigned long)0);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPublishFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "PUBLISH");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)26);
    EXPECT_EQ(frames[0].header_fields["dup"], (unsigned long)0);
    EXPECT_EQ(frames[0].header_fields["retain"], (unsigned long)0);
    EXPECT_EQ(frames[0].header_fields["qos"], (unsigned long)1);
    EXPECT_EQ(frames[0].payload["topic_name"], "test/topic");
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubackFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "PUBACK");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)3);
    EXPECT_EQ(frames[0].header_fields["packet_identifier"], (unsigned long)1);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrecFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "PUBREC");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)2);
    EXPECT_EQ(frames[0].header_fields["packet_identifier"], (unsigned long)1);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubrelFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "PUBREL");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)2);
    EXPECT_EQ(frames[0].header_fields["packet_identifier"], (unsigned long)1);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPubcompFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "PUBCOMP");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)2);
    EXPECT_EQ(frames[0].header_fields["packet_identifier"], (unsigned long)1);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubscribeFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "SUBSCRIBE");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)16);
    EXPECT_EQ(frames[0].header_fields["packet_identifier"], (unsigned long)1);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kSubackFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "SUBACK");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)4);
    EXPECT_EQ(frames[0].payload["reason_code"], "0");
    EXPECT_EQ(frames[0].header_fields["packet_identifier"], (unsigned long)1);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubscribeFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "UNSUBSCRIBE");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)15);
    EXPECT_EQ(frames[0].header_fields["packet_identifier"], (unsigned long)2);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kUnsubackFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "UNSUBACK");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)4);
    EXPECT_EQ(frames[0].header_fields["packet_identifier"], (unsigned long)2);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingreqFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "PINGREQ");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)0);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kPingrespFrame));
    parse_result = ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "PINGRESP");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)0);
    frames.clear();

    frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kDisconnectFrame));
    parse_result = ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);
    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
    EXPECT_EQ(frames[0].control_packet_type, "DISCONNECT");
    EXPECT_EQ(frames[0].header_fields["remaining_length"], (size_t)1);
    EXPECT_EQ(frames[0].header_fields["reason_code"], (unsigned long)4);
    frames.clear();
}

} // namespace mqtt
} // namespace protocols
} // namespace stirling
} // namespace px
