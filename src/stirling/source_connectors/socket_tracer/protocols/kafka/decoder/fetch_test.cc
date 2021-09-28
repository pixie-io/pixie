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
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/opcodes/message_set.h"

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

bool operator==(const FetchRespAbortedTransaction& lhs, const FetchRespAbortedTransaction& rhs) {
  return lhs.producer_id == rhs.producer_id && lhs.first_offset == rhs.first_offset;
}

bool operator!=(const FetchRespAbortedTransaction& lhs, const FetchRespAbortedTransaction& rhs) {
  return !(lhs == rhs);
}

bool operator==(const FetchRespPartition& lhs, const FetchRespPartition& rhs) {
  if (lhs.index != rhs.index || lhs.error_code != rhs.error_code ||
      lhs.high_watermark != rhs.high_watermark ||
      lhs.last_stable_offset != rhs.last_stable_offset ||
      lhs.log_start_offset != rhs.log_start_offset ||
      lhs.preferred_read_replica != rhs.preferred_read_replica) {
    return false;
  }
  if (lhs.aborted_transactions.size() != rhs.aborted_transactions.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.aborted_transactions.size(); ++i) {
    if (lhs.aborted_transactions[i] != rhs.aborted_transactions[i]) {
      return false;
    }
  }
  return lhs.message_set == rhs.message_set;
}

bool operator!=(const FetchRespPartition& lhs, const FetchRespPartition& rhs) {
  return !(lhs == rhs);
}

bool operator==(const FetchRespTopic& lhs, const FetchRespTopic& rhs) {
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

bool operator!=(const FetchRespTopic& lhs, const FetchRespTopic& rhs) { return !(lhs == rhs); }

bool operator==(const FetchResp& lhs, const FetchResp& rhs) {
  if (lhs.throttle_time_ms != rhs.throttle_time_ms || lhs.error_code != rhs.error_code ||
      lhs.session_id != rhs.session_id) {
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

TEST(KafkaPacketDecoder, TestExtractFetchReqV4) {
  const std::string_view input = CreateStringView<char>(
      "\xFF\xFF\xFF\xFF\x00\x00\x01\xF4\x00\x00\x00\x01\x03\x20\x00\x00\x00\x00\x00\x00\x01\x00"
      "\x08\x6D\x79\x2D\x74\x6F\x70\x69\x63\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x01\x7E\x00\x10\x00\x00");

  FetchReqPartition partition{
      .index = 0,
      .current_leader_epoch = -1,
      .fetch_offset = 382,
      .last_fetched_epoch = -1,
      .partition_max_bytes = 1048576,
  };
  FetchReqTopic topic{
      .name = "my-topic",
      .partitions = {partition},
  };
  FetchReq expected_result{.replica_id = -1,
                           .session_id = 0,
                           .session_epoch = -1,
                           .topics = {topic},
                           .forgotten_topics = {},
                           .rack_id = ""};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kFetch, 4);
  EXPECT_OK_AND_EQ(decoder.ExtractFetchReq(), expected_result);
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

TEST(KafkaPacketDecoder, TestExtractFetchRespV4) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x00\x01\x00\x08\x6D\x79\x2D\x74\x6F\x70\x69\x63\x00\x00\x00\x01"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x7E\x00\x00\x00\x00\x00\x00\x01\x7E\xFF"
      "\xFF\xFF\xFF\x00\x00\x00\x00");
  MessageSet message_set{.size = 0, .record_batches = {}};
  FetchRespPartition partition{
      .index = 0,
      .error_code = 0,
      .high_watermark = 382,
      .last_stable_offset = 382,
      .log_start_offset = -1,
      .aborted_transactions = {},
      .preferred_read_replica = -1,
      .message_set = message_set,
  };
  FetchRespTopic topic{
      .name = "my-topic",
      .partitions = {partition},
  };
  FetchResp expected_result{
      .throttle_time_ms = 0,
      .error_code = 0,
      .session_id = 0,
      .topics = {topic},
  };

  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kFetch, 4);
  EXPECT_OK_AND_EQ(decoder.ExtractFetchResp(), expected_result);
}

TEST(KafkaPacketDecoder, TestExtractFetchRespV11) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x27\xd5\xb6\xd1\x00\x00\x00\x01\x00\x11\x71\x75\x69\x63\x6b\x73\x74"
      "\x61\x72\x74\x2d\x65\x76\x65\x6e\x74\x73\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff"
      "\xff\xff\xff\xff\xff\xff\x00\x00\x01\x71\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x38\x00"
      "\x00\x00\x00\x02\x7e\x35\x4f\xcb\x00\x00\x00\x00\x00\x00\x00\x00\x01\x7a\xb0\x95\x78\xbc\x00"
      "\x00\x01\x7a\xb0\x95\x78\xbc\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00"
      "\x00\x01\x0c\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x38\x00\x00"
      "\x00\x00\x02\x1b\x91\x32\x93\x00\x00\x00\x00\x00\x00\x00\x00\x01\x7a\xb2\x08\x48\x52\x00\x00"
      "\x01\x7a\xb2\x08\x48\x52\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00"
      "\x01\x0c\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x38\x00\x00\x00"
      "\x00\x02\x99\x41\x19\xe9\x00\x00\x00\x00\x00\x00\x00\x00\x01\x7a\xb2\x08\xde\x56\x00\x00\x01"
      "\x7a\xb2\x08\xde\x56\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01"
      "\x0c\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x46\x00\x00\x00\x00"
      "\x02\xa7\x88\x71\xd8\x00\x00\x00\x00\x00\x00\x00\x00\x01\x7a\xb2\x0a\x70\x1d\x00\x00\x01\x7a"
      "\xb2\x0a\x70\x1d\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x28"
      "\x00\x00\x00\x01\x1c\x4d\x79\x20\x66\x69\x72\x73\x74\x20\x65\x76\x65\x6e\x74\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x04\x00\x00\x00\x47\x00\x00\x00\x00\x02\x5c\x9d\xc5\x05\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x01\x7a\xb2\x0a\xb7\xe5\x00\x00\x01\x7a\xb2\x0a\xb7\xe5\xff\xff\xff\xff\xff\xff"
      "\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x2a\x00\x00\x00\x01\x1e\x4d\x79\x20\x73\x65"
      "\x63\x6f\x6e\x64\x20\x65\x76\x65\x6e\x74\x00");
  RecordBatch record_batch1{{{.key = "", .value = ""}}};
  RecordBatch record_batch2{{{.key = "", .value = ""}}};
  RecordBatch record_batch3{{{.key = "", .value = ""}}};
  RecordBatch record_batch4{{{.key = "", .value = "My first event"}}};
  RecordBatch record_batch5{{{.key = "", .value = "My second event"}}};
  MessageSet message_set{.size = 369,
                         .record_batches = {record_batch1, record_batch2, record_batch3,
                                            record_batch4, record_batch5}};
  FetchRespPartition partition{
      .index = 0,
      .error_code = 0,
      .high_watermark = 5,
      .last_stable_offset = 5,
      .log_start_offset = 0,
      .aborted_transactions = {},
      .preferred_read_replica = -1,
      .message_set = message_set,
  };
  FetchRespTopic topic{
      .name = "quickstart-events",
      .partitions = {partition},
  };
  FetchResp expected_result{
      .throttle_time_ms = 0,
      .error_code = 0,
      .session_id = 668317393,
      .topics = {topic},
  };

  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kFetch, 11);
  EXPECT_OK_AND_EQ(decoder.ExtractFetchResp(), expected_result);
}

TEST(KafkaPacketDecoder, TestExtractFetchRespV12) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x27\xd5\xb6\xd1\x02\x12\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d"
      "\x65\x76\x65\x6e\x74\x73\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00"
      "\x00\x00\x00\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00\x01\xff\xff\xff\xff\xf2\x02\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x38\x00\x00\x00\x00\x02\x7e\x35\x4f\xcb\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x01\x7a\xb0\x95\x78\xbc\x00\x00\x01\x7a\xb0\x95\x78\xbc\xff\xff\xff\xff\xff"
      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x0c\x00\x00\x00\x01\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x01\x00\x00\x00\x38\x00\x00\x00\x00\x02\x1b\x91\x32\x93\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x01\x7a\xb2\x08\x48\x52\x00\x00\x01\x7a\xb2\x08\x48\x52\xff\xff\xff\xff\xff\xff"
      "\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x0c\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x02\x00\x00\x00\x38\x00\x00\x00\x00\x02\x99\x41\x19\xe9\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x01\x7a\xb2\x08\xde\x56\x00\x00\x01\x7a\xb2\x08\xde\x56\xff\xff\xff\xff\xff\xff\xff"
      "\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x0c\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x03\x00\x00\x00\x46\x00\x00\x00\x00\x02\xa7\x88\x71\xd8\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x01\x7a\xb2\x0a\x70\x1d\x00\x00\x01\x7a\xb2\x0a\x70\x1d\xff\xff\xff\xff\xff\xff\xff\xff"
      "\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x28\x00\x00\x00\x01\x1c\x4d\x79\x20\x66\x69\x72\x73"
      "\x74\x20\x65\x76\x65\x6e\x74\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x47\x00\x00\x00"
      "\x00\x02\x5c\x9d\xc5\x05\x00\x00\x00\x00\x00\x00\x00\x00\x01\x7a\xb2\x0a\xb7\xe5\x00\x00\x01"
      "\x7a\xb2\x0a\xb7\xe5\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01"
      "\x2a\x00\x00\x00\x01\x1e\x4d\x79\x20\x73\x65\x63\x6f\x6e\x64\x20\x65\x76\x65\x6e\x74\x00\x00"
      "\x00\x00");
  RecordBatch record_batch1{{{.key = "", .value = ""}}};
  RecordBatch record_batch2{{{.key = "", .value = ""}}};
  RecordBatch record_batch3{{{.key = "", .value = ""}}};
  RecordBatch record_batch4{{{.key = "", .value = "My first event"}}};
  RecordBatch record_batch5{{{.key = "", .value = "My second event"}}};
  MessageSet message_set{.size = 369,
                         .record_batches = {record_batch1, record_batch2, record_batch3,
                                            record_batch4, record_batch5}};
  FetchRespPartition partition{
      .index = 0,
      .error_code = 0,
      .high_watermark = 5,
      .last_stable_offset = 5,
      .log_start_offset = 0,
      .aborted_transactions = {},
      .preferred_read_replica = -1,
      .message_set = message_set,
  };
  FetchRespTopic topic{
      .name = "quickstart-events",
      .partitions = {partition},
  };
  FetchResp expected_result{
      .throttle_time_ms = 0,
      .error_code = 0,
      .session_id = 668317393,
      .topics = {topic},
  };

  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kFetch, 12);
  EXPECT_OK_AND_EQ(decoder.ExtractFetchResp(), expected_result);
}

TEST(KafkaPacketDecoder, TestExtractFetchRespV11MissingMessageSet) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x27\xd5\xb6\xd1\x00\x00\x00\x01\x00\x11\x71\x75\x69\x63\x6b\x73\x74"
      "\x61\x72\x74\x2d\x65\x76\x65\x6e\x74\x73\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff"
      "\xff\xff\xff\xff\xff\xff\x00\x00\x01\x71\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00");
  MessageSet message_set{.size = 369, .record_batches = {}};
  FetchRespPartition partition{
      .index = 0,
      .error_code = 0,
      .high_watermark = 5,
      .last_stable_offset = 5,
      .log_start_offset = 0,
      .aborted_transactions = {},
      .preferred_read_replica = -1,
      .message_set = message_set,
  };
  FetchRespTopic topic{
      .name = "quickstart-events",
      .partitions = {partition},
  };
  FetchResp expected_result{
      .throttle_time_ms = 0,
      .error_code = 0,
      .session_id = 668317393,
      .topics = {topic},
  };

  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kFetch, 11);
  EXPECT_OK_AND_EQ(decoder.ExtractFetchResp(), expected_result);
}

TEST(KafkaPacketDecoder, TestExtractFetchRespV12MissingMessageSet) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x27\xd5\xb6\xd1\x02\x12\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d"
      "\x65\x76\x65\x6e\x74\x73\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00"
      "\x00\x00\x00\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00\x01\xff\xff\xff\xff\xf2\x02\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00");
  MessageSet message_set{.size = 369, .record_batches = {}};
  FetchRespPartition partition{
      .index = 0,
      .error_code = 0,
      .high_watermark = 5,
      .last_stable_offset = 5,
      .log_start_offset = 0,
      .aborted_transactions = {},
      .preferred_read_replica = -1,
      .message_set = message_set,
  };
  FetchRespTopic topic{
      .name = "quickstart-events",
      .partitions = {partition},
  };
  FetchResp expected_result{
      .throttle_time_ms = 0,
      .error_code = 0,
      .session_id = 668317393,
      .topics = {topic},
  };

  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kFetch, 12);
  EXPECT_OK_AND_EQ(decoder.ExtractFetchResp(), expected_result);
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
