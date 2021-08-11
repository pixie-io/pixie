/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache LicenseVersion 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writingsoftware
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KINDeither express or implied.
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

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::px::operator<<;

bool operator==(const FetchReqPartition& lhs, const FetchReqPartition& rhs) {
  return lhs.index == rhs.index && lhs.current_leader_epoch == rhs.current_leader_epoch &&
         lhs.fetch_offset == rhs.fetch_offset && lhs.last_fetched_epoch == rhs.last_fetched_epoch &&
         lhs.log_start_offset == rhs.log_start_offset &&
         lhs.partition_max_bytes == rhs.partition_max_bytes;
}

bool operator!=(const FetchReqPartition& lhs, const FetchReqPartition& rhs) {
  return !(lhs == rhs);
}

bool operator==(const FetchReqTopic& lhs, const FetchReqTopic& rhs) {
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

bool operator!=(const FetchReqTopic& lhs, const FetchReqTopic& rhs) { return !(lhs == rhs); }

bool operator==(const FetchForgottenTopicsData& lhs, const FetchForgottenTopicsData& rhs) {
  if (lhs.name != rhs.name) {
    return false;
  }
  if (lhs.partition_indices.size() != rhs.partition_indices.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.partition_indices.size(); ++i) {
    if (lhs.partition_indices[i] != rhs.partition_indices[i]) {
      return false;
    }
  }
  return true;
}

bool operator!=(const FetchForgottenTopicsData& lhs, const FetchForgottenTopicsData& rhs) {
  return !(lhs == rhs);
}

bool operator==(const FetchReq& lhs, const FetchReq& rhs) {
  if (lhs.replica_id != rhs.replica_id) {
    return false;
  }
  if (lhs.session_id != rhs.session_id) {
    return false;
  }
  if (lhs.session_epoch != rhs.session_epoch) {
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
  if (lhs.forgotten_topics.size() != rhs.forgotten_topics.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.forgotten_topics.size(); ++i) {
    if (lhs.forgotten_topics[i] != rhs.forgotten_topics[i]) {
      return false;
    }
  }
  return lhs.rack_id == rhs.rack_id;
}

TEST(KafkaPacketDecoder, TestExtractFetchReqV11) {
  const std::string_view input = CreateStringView<char>(
      "\xff\xff\xff\xff\x00\x00\x01\xf4\x00\x00\x00\x01\x03\x20\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x01\x00\x11\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65\x76\x65"
      "\x6e\x74\x73\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\xff\xff\xff\xff\xff\xff\xff\xff\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00");

  FetchReqPartition partition{
      .index = 0,
      .current_leader_epoch = 0,
      .fetch_offset = 0,
      .log_start_offset = -1,
      .partition_max_bytes = 1048576,
  };
  FetchReqTopic topic{
      .name = "quickstart-events",
      .partitions = {partition},
  };
  FetchReq expected_result{
      .replica_id = -1,
      .session_id = 0,
      .session_epoch = 0,
      .topics = {topic},
      .forgotten_topics = {},
      .rack_id = "",
  };
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kFetch, 11);
  EXPECT_OK_AND_EQ(decoder.ExtractFetchReq(), expected_result);
}

TEST(KafkaPacketDecoder, TestExtractFetchReqV12) {
  const std::string_view input = CreateStringView<char>(
      "\xff\xff\xff\xff\x00\x00\x01\xf4\x00\x00\x00\x01\x03\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x02\x12\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65\x76\x65\x6e\x74\x73\x02\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff"
      "\xff\xff\xff\xff\x00\x10\x00\x00\x00\x00\x01\x01\x00");

  FetchReqPartition partition{
      .index = 0,
      .current_leader_epoch = 0,
      .fetch_offset = 0,
      .log_start_offset = -1,
      .partition_max_bytes = 1048576,
  };
  FetchReqTopic topic{
      .name = "quickstart-events",
      .partitions = {partition},
  };
  FetchReq expected_result{
      .replica_id = -1,
      .session_id = 0,
      .session_epoch = 0,
      .topics = {topic},
      .forgotten_topics = {},
      .rack_id = "",
  };
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kFetch, 12);
  EXPECT_OK_AND_EQ(decoder.ExtractFetchReq(), expected_result);
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
