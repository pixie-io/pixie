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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/packet_decoder.h"
#include <utility>
#include <vector>
#include "src/common/base/types.h"
#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::px::operator<<;

bool operator==(const RecordMessage& lhs, const RecordMessage& rhs) {
  return lhs.key == rhs.key && lhs.value == rhs.value;
}

bool operator!=(const RecordMessage& lhs, const RecordMessage& rhs) { return !(lhs == rhs); }

bool operator==(const RecordBatch& lhs, const RecordBatch& rhs) {
  if (lhs.records.size() != rhs.records.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.records.size(); ++i) {
    if (lhs.records[i] != rhs.records[i]) {
      return false;
    }
  }
  return true;
}

bool operator==(const ProduceReqPartition& lhs, const ProduceReqPartition& rhs) {
  return lhs.index == rhs.index && lhs.record_batch == rhs.record_batch;
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

template <typename T>
struct PacketDecoderTestCase {
  std::string input;
  T expected_output;
};

template <typename T>
class PacketDecoderTest : public ::testing::TestWithParam<PacketDecoderTestCase<T>> {};

class PacketDecoderTestUnsignedVarint : public PacketDecoderTest<int32_t> {};

TEST_P(PacketDecoderTestUnsignedVarint, ExtractUnsignedVarint) {
  PacketDecoder decoder = PacketDecoder(GetParam().input);
  EXPECT_OK_AND_EQ(decoder.ExtractUnsignedVarint(), GetParam().expected_output);
}

INSTANTIATE_TEST_SUITE_P(
    AllData, PacketDecoderTestUnsignedVarint,
    ::testing::Values(
        PacketDecoderTestCase<int32_t>{std::string("\x00", 1), 0},
        PacketDecoderTestCase<int32_t>{std::string("\x03", 1), 3},
        PacketDecoderTestCase<int32_t>{std::string("\x96\x01", 2), 150},
        PacketDecoderTestCase<int32_t>{std::string("\xff\xff\xff\xff\x0f", 5), -1},
        PacketDecoderTestCase<int32_t>{std::string("\x80\xC0\xFF\xFF\x0F", 5), -8192},
        PacketDecoderTestCase<int32_t>{std::string("\xff\xff\xff\xff\x07", 5), INT_MAX},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x80\x80\x80\x08", 5), INT_MIN}));

class PacketDecoderTestVarint : public PacketDecoderTest<int32_t> {};

TEST_P(PacketDecoderTestVarint, ExtractVarint) {
  PacketDecoder decoder = PacketDecoder(GetParam().input);
  EXPECT_OK_AND_EQ(decoder.ExtractVarint(), GetParam().expected_output);
}

INSTANTIATE_TEST_SUITE_P(
    AllData, PacketDecoderTestVarint,
    ::testing::Values(
        PacketDecoderTestCase<int32_t>{std::string("\x00", 1), 0},
        PacketDecoderTestCase<int32_t>{std::string("\x01", 1), -1},
        PacketDecoderTestCase<int32_t>{std::string("\x02", 1), 1},
        PacketDecoderTestCase<int32_t>{std::string("\x7E", 1), 63},
        PacketDecoderTestCase<int32_t>{std::string("\x7F", 1), -64},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x01", 2), 64},
        PacketDecoderTestCase<int32_t>{std::string("\x81\x01", 2), -65},
        PacketDecoderTestCase<int32_t>{std::string("\xFE\x7F", 2), 8191},
        PacketDecoderTestCase<int32_t>{std::string("\xFF\x7F", 2), -8192},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x80\x01", 3), 8192},
        PacketDecoderTestCase<int32_t>{std::string("\x81\x80\x01", 3), -8193},
        PacketDecoderTestCase<int32_t>{std::string("\xFE\xFF\x7F", 3), 1048575},
        PacketDecoderTestCase<int32_t>{std::string("\xFF\xFF\x7F", 3), -1048576},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x80\x80\x01", 4), 1048576},
        PacketDecoderTestCase<int32_t>{std::string("\x81\x80\x80\x01", 4), -1048577},
        PacketDecoderTestCase<int32_t>{std::string("\xFE\xFF\xFF\x7F", 4), 134217727},
        PacketDecoderTestCase<int32_t>{std::string("\xFF\xFF\xFF\x7F", 4), -134217728},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x80\x80\x80\x01", 5), 134217728},
        PacketDecoderTestCase<int32_t>{std::string("\x81\x80\x80\x80\x01", 5), -134217729},
        PacketDecoderTestCase<int32_t>{std::string("\xFE\xFF\xFF\xFF\x0F", 5), INT_MAX},
        PacketDecoderTestCase<int32_t>{std::string("\xFF\xFF\xFF\xFF\x0F", 5), INT_MIN}));

class PacketDecoderTestVarlong : public PacketDecoderTest<int64_t> {};

TEST_P(PacketDecoderTestVarlong, ExtractVarlong) {
  PacketDecoder decoder = PacketDecoder(GetParam().input);
  EXPECT_OK_AND_EQ(decoder.ExtractVarlong(), GetParam().expected_output);
}

INSTANTIATE_TEST_SUITE_P(
    AllData, PacketDecoderTestVarlong,
    ::testing::Values(
        PacketDecoderTestCase<int64_t>{std::string("\x00", 1), 0},
        PacketDecoderTestCase<int64_t>{std::string("\x01", 1), -1},
        PacketDecoderTestCase<int64_t>{std::string("\x02", 1), 1},
        PacketDecoderTestCase<int64_t>{std::string("\x7E", 1), 63},
        PacketDecoderTestCase<int64_t>{std::string("\x7F", 1), -64},
        PacketDecoderTestCase<int64_t>{std::string("\x80\x01", 2), 64},
        PacketDecoderTestCase<int64_t>{std::string("\x81\x01", 2), -65},
        PacketDecoderTestCase<int64_t>{std::string("\xFE\x7F", 2), 8191},
        PacketDecoderTestCase<int64_t>{std::string("\xFF\x7F", 2), -8192},
        PacketDecoderTestCase<int64_t>{std::string("\x80\x80\x01", 3), 8192},
        PacketDecoderTestCase<int64_t>{std::string("\x81\x80\x01", 3), -8193},
        PacketDecoderTestCase<int64_t>{std::string("\xFE\xFF\x7F", 3), 1048575},
        PacketDecoderTestCase<int64_t>{std::string("\xFF\xFF\x7F", 3), -1048576},
        PacketDecoderTestCase<int64_t>{std::string("\x80\x80\x80\x01", 4), 1048576},
        PacketDecoderTestCase<int64_t>{std::string("\x81\x80\x80\x01", 4), -1048577},
        PacketDecoderTestCase<int64_t>{std::string("\xFE\xFF\xFF\x7F", 4), 134217727},
        PacketDecoderTestCase<int64_t>{std::string("\xFF\xFF\xFF\x7F", 4), -134217728},
        PacketDecoderTestCase<int64_t>{std::string("\x80\x80\x80\x80\x01", 5), 134217728},
        PacketDecoderTestCase<int64_t>{std::string("\x81\x80\x80\x80\x01", 5), -134217729},
        PacketDecoderTestCase<int64_t>{std::string("\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01", 10),
                                       LONG_MAX},
        PacketDecoderTestCase<int64_t>{std::string("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01", 10),
                                       LONG_MIN}));

TEST(KafkaPacketDecoderTest, ExtractCompactString) {
  {
    const std::string_view msg = CreateStringView<char>(
        "\x3fHello world.This is a test string with 62 characters.PixieLabs");
    PacketDecoder decoder(msg);
    EXPECT_OK_AND_EQ(decoder.ExtractCompactString(), msg.substr(1));
  }

  // Test invalid length.
  {
    const std::string_view msg = CreateStringView<char>("\x00Hello world.");
    PacketDecoder decoder(msg);
    EXPECT_FALSE(decoder.ExtractCompactString().ok());
  }
}

TEST(KafkaPacketDecoderTest, ExtractCompactNullableString) {
  {
    const std::string_view msg = CreateStringView<char>(
        "\x3fHello world.This is a test string with 62 characters.PixieLabs");
    PacketDecoder decoder(msg);
    EXPECT_OK_AND_EQ(decoder.ExtractCompactNullableString(), msg.substr(1));
  }

  // Test null string.
  {
    const std::string_view msg = CreateStringView<char>("\x00");
    PacketDecoder decoder(msg);
    EXPECT_OK_AND_EQ(decoder.ExtractCompactNullableString(), "");
  }
}

TEST(KafkaPacketDecoderTest, ExtractArray) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x05"
      "\x00\x00\x00\x01"
      "\x00\x00\x00\x02"
      "\x00\x00\x00\x03"
      "\x00\x00\x00\x04"
      "\x00\x00\x00\x05");

  PacketDecoder decoder = PacketDecoder(input);
  EXPECT_OK_AND_THAT(decoder.ExtractArray(&PacketDecoder::ExtractInt32),
                     ElementsAre(1, 2, 3, 4, 5));
}

TEST(KafkaPacketDecoderTest, ExtractCompactArray) {
  // Test null array.
  {
    const std::string_view input = CreateStringView<char>("\x00");
    PacketDecoder decoder = PacketDecoder(input);
    EXPECT_OK_AND_THAT(decoder.ExtractCompactArray(&PacketDecoder::ExtractInt32), IsEmpty());
  }

  // Test array of strings.
  {
    const std::string_view input = CreateStringView<char>(
        "\x03"
        "\x00\x05Hello"
        "\x00\x06World!");
    PacketDecoder decoder(input);
    EXPECT_OK_AND_THAT(decoder.ExtractCompactArray(&PacketDecoder::ExtractString),
                       ElementsAre("Hello", "World!"));
  }
}

TEST(KafkaPacketDecoderTest, ExtractRecordMessage) {
  // Empty key and value Record.
  {
    std::string_view input = CreateStringView<char>("\x0c\x00\x00\x00\x01\x00\x00");
    RecordMessage expected_result{};
    PacketDecoder decoder(input);
    EXPECT_OK_AND_EQ(decoder.ExtractRecordMessage(), expected_result);
  }
  {
    std::string_view input =
        CreateStringView<char>("\x28\x00\x00\x00\x06key\x1cMy first event\x00");
    RecordMessage expected_result{.key = "key", .value = "My first event"};
    PacketDecoder decoder(input);
    EXPECT_OK_AND_EQ(decoder.ExtractRecordMessage(), expected_result);
  }
}

TEST(KafkaPacketDecoderTest, ExtractRecordBatchV8) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x46\xff\xff\xff\xff\x02\xa7\x88\x71\xd8\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x01\x7a\xb2\x0a\x70\x1d\x00\x00\x01\x7a\xb2\x0a\x70\x1d\xff"
      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x28\x00\x00\x00\x01"
      "\x1c\x4d\x79\x20\x66\x69\x72\x73\x74\x20\x65\x76\x65\x6e\x74\x00");
  RecordBatch expected_result{{{.key = "", .value = "My first event"}}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 8);
  EXPECT_OK_AND_EQ(decoder.ExtractRecordBatch(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractRecordBatchV9) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x4e\xff\xff\xff\xff\x02\xc0\xde\x91\x11\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x01\x7a\x1b\xc8\x2d\xaa\x00\x00\x01\x7a\x1b\xc8\x2d\xaa\xff"
      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x38\x00\x00\x00\x01"
      "\x2c\x54\x68\x69\x73\x20\x69\x73\x20\x6d\x79\x20\x66\x69\x72\x73\x74\x20\x65\x76\x65\x6e"
      "\x74\x00\x00\x00\x00");
  RecordBatch expected_result{{{.key = "", .value = "This is my first event"}}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 9);
  EXPECT_OK_AND_EQ(decoder.ExtractRecordBatch(), expected_result);
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
  ProduceReqPartition partition{.index = 0, .record_batch = record_batch};
  ProduceReqTopic topic{.name = "quickstart-events", .partitions = {partition}};
  ProduceReq expected_result{
      .transactional_id = "", .acks = 1, .timeout_ms = 1500, .topics = {topic}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 8);
  EXPECT_OK_AND_EQ(decoder.ExtractProduceReq(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractProduceReqV9) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x01\x00\x00\x05\xdc\x02\x12\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65"
      "\x76\x65\x6e\x74\x73\x02\x00\x00\x00\x00\x5b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x4e\xff\xff\xff\xff\x02\xc0\xde\x91\x11\x00\x00\x00\x00\x00\x00\x00\x00\x01\x7a\x1b\xc8"
      "\x2d\xaa\x00\x00\x01\x7a\x1b\xc8\x2d\xaa\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
      "\xff\xff\x00\x00\x00\x01\x38\x00\x00\x00\x01\x2c\x54\x68\x69\x73\x20\x69\x73\x20\x6d\x79"
      "\x20\x66\x69\x72\x73\x74\x20\x65\x76\x65\x6e\x74\x00\x00\x00\x00");
  RecordBatch record_batch{{{.key = "", .value = "This is my first event"}}};
  ProduceReqPartition partition{.index = 0, .record_batch = record_batch};
  ProduceReqTopic topic{.name = "quickstart-events", .partitions = {partition}};
  ProduceReq expected_result{
      .transactional_id = "", .acks = 1, .timeout_ms = 1500, .topics = {topic}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 9);
  EXPECT_OK_AND_EQ(decoder.ExtractProduceReq(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractProduceRespV8) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x01\x00\x11\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65\x76\x65\x6e\x74"
      "\x73\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\xff\xff\xff"
      "\xff\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x00\x00\x00"
      "\x00");
  ProduceRespPartition partition{
      .index = 0, .error_code = 0, .record_errors = {}, .error_message = ""};
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
  ProduceRespPartition partition{
      .index = 0, .error_code = 0, .record_errors = {}, .error_message = ""};
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
