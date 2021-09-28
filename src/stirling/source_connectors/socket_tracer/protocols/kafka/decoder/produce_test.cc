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

#include <utility>
#include <vector>
#include "src/common/base/types.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/decoder/packet_decoder.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/opcodes/message_set.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::px::operator<<;

bool operator==(const ProduceReqPartition& lhs, const ProduceReqPartition& rhs) {
  return lhs.index == rhs.index && lhs.message_set == rhs.message_set;
}

bool operator!=(const ProduceReqPartition& lhs, const ProduceReqPartition& rhs) {
  return !(lhs == rhs);
}

bool operator==(const ProduceReqTopic& lhs, const ProduceReqTopic& rhs) {
  if (lhs.name != rhs.name) {
    return false;
  }
  if (lhs.partitions.size() != rhs.partitions.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.partitions.size(); ++i) {
    if (lhs.partitions[i] != rhs.partitions[i]) {
      return false;
    }
  }
  return true;
}

bool operator!=(const ProduceReqTopic& lhs, const ProduceReqTopic& rhs) { return !(lhs == rhs); }

bool operator==(const ProduceReq& lhs, const ProduceReq& rhs) {
  if (lhs.transactional_id != rhs.transactional_id) {
    return false;
  }
  if (lhs.acks != rhs.acks) {
    return false;
  }
  if (lhs.timeout_ms != rhs.timeout_ms) {
    return false;
  }
  if (lhs.topics.size() != rhs.topics.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.topics.size(); ++i) {
    if (lhs.topics[i] != rhs.topics[i]) {
      return false;
    }
  }
  return true;
}

bool operator==(const RecordError& lhs, const RecordError& rhs) {
  return lhs.batch_index == rhs.batch_index && lhs.error_message == rhs.error_message;
}

bool operator!=(const RecordError& lhs, const RecordError& rhs) { return !(lhs == rhs); }

bool operator==(const ProduceRespPartition& lhs, const ProduceRespPartition& rhs) {
  if (lhs.index != rhs.index) {
    return false;
  }
  if (lhs.error_code != rhs.error_code) {
    return false;
  }
  if (lhs.base_offset != rhs.base_offset) {
    return false;
  }
  if (lhs.log_append_time_ms != rhs.log_append_time_ms) {
    return false;
  }
  if (lhs.log_start_offset != rhs.log_start_offset) {
    return false;
  }
  if (lhs.error_message != rhs.error_message) {
    return false;
  }

  if (lhs.record_errors.size() != rhs.record_errors.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.record_errors.size(); ++i) {
    if (lhs.record_errors[i] != rhs.record_errors[i]) {
      return false;
    }
  }
  return true;
}

bool operator!=(const ProduceRespPartition& lhs, const ProduceRespPartition& rhs) {
  return !(lhs == rhs);
}

bool operator==(const ProduceRespTopic& lhs, const ProduceRespTopic& rhs) {
  if (lhs.name != rhs.name) {
    return false;
  }
  if (lhs.partitions.size() != rhs.partitions.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.partitions.size(); ++i) {
    if (lhs.partitions[i] != rhs.partitions[i]) {
      return false;
    }
  }
  return true;
}

bool operator!=(const ProduceRespTopic& lhs, const ProduceRespTopic& rhs) { return !(lhs == rhs); }

bool operator==(const ProduceResp& lhs, const ProduceResp& rhs) {
  if (lhs.throttle_time_ms != rhs.throttle_time_ms) {
    return false;
  }
  if (lhs.topics.size() != rhs.topics.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.topics.size(); ++i) {
    if (lhs.topics[i] != rhs.topics[i]) {
      return false;
    }
  }
  return true;
}

TEST(KafkaPacketDecoderTest, ExtractProduceReqV7) {
  const std::string_view input = CreateStringView<char>(
      "\xFF\xFF\x00\x01\x00\x00\x75\x30\x00\x00\x00\x01\x00\x08\x6D\x79\x2D\x74\x6F\x70\x69\x63\x00"
      "\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x5C\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x50"
      "\x00\x00\x00\x00\x02\x76\x7C\xA6\x2F\x00\x00\x00\x00\x00\x01\x00\x00\x01\x7C\x29\x89\x9A\xA2"
      "\x00\x00\x01\x7C\x29\x89\x9A\xA2\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00"
      "\x00\x00\x02\x14\x00\x00\x00\x01\x08\x74\x65\x73\x74\x00\x26\x00\x00\x02\x01\x1A\xC2\x48\x6F"
      "\x6C\x61\x2C\x20\x6D\x75\x6E\x64\x6F\x21\x00");
  RecordBatch record_batch{
      .records = {{.key = "", .value = "test"}, {.key = "", .value = "\xc2Hola, mundo!"}}};
  MessageSet message_set{.size = 92, .record_batches = {record_batch}};
  ProduceReqPartition partition{.index = 0, .message_set = message_set};
  ProduceReqTopic topic{.name = "my-topic", .partitions = {partition}};
  ProduceReq expected_result{
      .transactional_id = "", .acks = 1, .timeout_ms = 30000, .topics = {topic}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 7);
  EXPECT_OK_AND_EQ(decoder.ExtractProduceReq(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractProduceReqV8) {
  const std::string_view input = CreateStringView<char>(
      "\xff\xff\x00\x01\x00\x00\x05\xdc\x00\x00\x00\x01\x00\x11\x71\x75\x69\x63\x6b\x73\x74\x61"
      "\x72\x74\x2d\x65\x76\x65\x6e\x74\x73\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x52\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x46\xff\xff\xff\xff\x02\xa7\x88\x71\xd8\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x01\x7a\xb2\x0a\x70\x1d\x00\x00\x01\x7a\xb2\x0a\x70\x1d\xff\xff"
      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x28\x00\x00\x00\x01\x1c"
      "\x4d\x79\x20\x66\x69\x72\x73\x74\x20\x65\x76\x65\x6e\x74\x00");
  RecordBatch record_batch{{{.key = "", .value = "My first event"}}};
  MessageSet message_set{.size = 70, .record_batches = {record_batch}};
  ProduceReqPartition partition{.index = 0, .message_set = message_set};
  ProduceReqTopic topic{.name = "quickstart-events", .partitions = {partition}};
  ProduceReq expected_result{
      .transactional_id = "", .acks = 1, .timeout_ms = 1500, .topics = {topic}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 8);
  EXPECT_OK_AND_EQ(decoder.ExtractProduceReq(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractProduceReqV9) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x01\x00\x00\x05\xdc\x02\x12\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65"
      "\x76\x65\x6e\x74\x73\x02\x00\x00\x00\x00\x5b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x4e\xff\xff\xff\xff\x02\xc0\xde\x91\x11\x00\x00\x00\x00\x00\x00\x00\x00\x01\x7a\x1b\xc8"
      "\x2d\xaa\x00\x00\x01\x7a\x1b\xc8\x2d\xaa\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
      "\xff\xff\x00\x00\x00\x01\x38\x00\x00\x00\x01\x2c\x54\x68\x69\x73\x20\x69\x73\x20\x6d\x79"
      "\x20\x66\x69\x72\x73\x74\x20\x65\x76\x65\x6e\x74\x00\x00\x00\x00");
  RecordBatch record_batch{{{.key = "", .value = "This is my first event"}}};
  MessageSet message_set{.size = 91, .record_batches = {record_batch}};
  ProduceReqPartition partition{.index = 0, .message_set = message_set};
  ProduceReqTopic topic{.name = "quickstart-events", .partitions = {partition}};
  ProduceReq expected_result{
      .transactional_id = "", .acks = 1, .timeout_ms = 1500, .topics = {topic}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 9);
  EXPECT_OK_AND_EQ(decoder.ExtractProduceReq(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractProduceRespV7) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x01\x00\x08\x6D\x79\x2D\x74\x6F\x70\x69\x63\x00\x00\x00\x01\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x01\xAE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00");
  ProduceRespPartition partition{.index = 0,
                                 .error_code = 0,
                                 .base_offset = 430,
                                 .log_append_time_ms = -1,
                                 .log_start_offset = 0,
                                 .record_errors = {},
                                 .error_message = ""};
  ProduceRespTopic topic{.name = "my-topic", .partitions = {partition}};
  ProduceResp expected_result{.topics = {topic}, .throttle_time_ms = 0};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 7);
  EXPECT_OK_AND_EQ(decoder.ExtractProduceResp(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractProduceRespV8) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x01\x00\x11\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65\x76\x65\x6e\x74"
      "\x73\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\xff\xff\xff"
      "\xff\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x00\x00\x00"
      "\x00");
  ProduceRespPartition partition{.index = 0,
                                 .error_code = 0,
                                 .base_offset = 3,
                                 .log_append_time_ms = -1,
                                 .log_start_offset = 0,
                                 .record_errors = {},
                                 .error_message = ""};
  ProduceRespTopic topic{.name = "quickstart-events", .partitions = {partition}};
  ProduceResp expected_result{.topics = {topic}, .throttle_time_ms = 0};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 8);
  EXPECT_OK_AND_EQ(decoder.ExtractProduceResp(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractProduceRespV9) {
  const std::string_view input = CreateStringView<char>(
      "\x02\x12\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65\x76\x65\x6e\x74\x73\x02\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00");
  ProduceRespPartition partition{.index = 0,
                                 .error_code = 0,
                                 .base_offset = 0,
                                 .log_append_time_ms = -1,
                                 .log_start_offset = 0,
                                 .record_errors = {},
                                 .error_message = ""};
  ProduceRespTopic topic{.name = "quickstart-events", .partitions = {partition}};
  ProduceResp expected_result{.topics = {topic}, .throttle_time_ms = 0};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 9);
  EXPECT_OK_AND_EQ(decoder.ExtractProduceResp(), expected_result);
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
