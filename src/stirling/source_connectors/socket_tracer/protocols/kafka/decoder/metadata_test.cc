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

template <typename T>
bool vectors_equal(std::vector<T> const& lhs, std::vector<T> const& rhs) {
  return (lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin()));
}
bool operator==(const MetadataReqTopic& lhs, const MetadataReqTopic& rhs) {
  return lhs.topic_id == rhs.topic_id && lhs.name == rhs.name;
}

bool operator!=(const MetadataReqTopic& lhs, const MetadataReqTopic& rhs) { return !(lhs == rhs); }

bool operator==(const MetadataReq& lhs, const MetadataReq& rhs) {
  return lhs.allow_auto_topic_creation == rhs.allow_auto_topic_creation &&
         lhs.include_cluster_authorized_operations == rhs.include_cluster_authorized_operations &&
         lhs.include_topic_authorized_operations == rhs.include_topic_authorized_operations &&
         vectors_equal(lhs.topics, rhs.topics);
}

bool operator!=(const MetadataReq& lhs, const MetadataReq& rhs) { return !(lhs == rhs); }

TEST(KafkaPacketDecoder, TestExtractMetadataReqV5) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x01\x00\x10\x6b\x61"
      "\x66\x6b\x61\x5f\x32\x2e\x31\x32\x2d\x31\x2e\x31\x2e\x31\x01");

  MetadataReqTopic topic{.topic_id = "", .name = "kafka_2.12-1.1.1"};
  MetadataReq expected_result{.topics = {topic}, .allow_auto_topic_creation = true};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kMetadata, 5);
  EXPECT_OK_AND_EQ(decoder.ExtractMetadataReq(), expected_result);
}

TEST(KafkaPacketDecoder, TestExtractMetadataReqV11) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x11\x6b\x61\x66\x6b\x61\x5f\x32\x2e\x31\x32\x2d\x32\x2e\x38"
      "\x2e\x31\x00\x01\x00\x00");

  MetadataReq expected_result{.topics = {},
                              .allow_auto_topic_creation = true,
                              .include_cluster_authorized_operations = false,
                              .include_topic_authorized_operations = false};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kMetadata, 11);

  EXPECT_OK_AND_EQ(decoder.ExtractMetadataReq(), expected_result);
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
