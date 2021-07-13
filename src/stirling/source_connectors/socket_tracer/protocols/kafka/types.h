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

#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

// API Keys (opcodes)
// Before each request is sent, the client sends the API key and the API version.These two 16-bit
// numbers, when taken together, uniquely identify the schema of the message to follow.
// https://kafka.apache.org/protocol.html#protocol_api_keys
enum class APIKey : int16_t {
  kProduce = 0,
  kFetch = 1,
  kListOffsets = 2,
  kMetadata = 3,
  kLeaderAndIsr = 4,
  kStopReplica = 5,
  kUpdateMetadata = 6,
  kControlledShutdown = 7,
  kOffsetCommit = 8,
  kOffsetFetch = 9,
  kFindCoordinator = 10,
  kJoinGroup = 11,
  kHeartbeat = 12,
  kLeaveGroup = 13,
  kSyncGroup = 14,
  kDescribeGroups = 15,
  kListGroups = 16,
  kSaslHandshake = 17,
  kApiVersions = 18,
  kCreateTopics = 19,
  kDeleteTopics = 20,
  kDeleteRecords = 21,
  kInitProducerId = 22,
  kOffsetForLeaderEpoch = 23,
  kAddPartitionsToTxn = 24,
  kAddOffsetsToTxn = 25,
  kEndTxn = 26,
  kWriteTxnMarkers = 27,
  kTxnOffsetCommit = 28,
  kDescribeAcls = 29,
  kCreateAcls = 30,
  kDeleteAcls = 31,
  kDescribeConfigs = 32,
  kAlterConfigs = 33,
  kAlterReplicaLogDirs = 34,
  kDescribeLogDirs = 35,
  kSaslAuthenticate = 36,
  kCreatePartitions = 37,
  kCreateDelegationToken = 38,
  kRenewDelegationToken = 39,
  kExpireDelegationToken = 40,
  kDescribeDelegationToken = 41,
  kDeleteGroups = 42,
  kElectLeaders = 43,
  kIncrementalAlterConfigs = 44,
  kAlterPartitionReassignments = 45,
  kListPartitionReassignments = 46,
  kOffsetDelete = 47,
  kDescribeClientQuotas = 48,
  kAlterClientQuotas = 49,
  kDescribeUserScramCredentials = 50,
  kAlterUserScramCredentials = 51,
  kAlterIsr = 56,
  kUpdateFeatures = 57,
  kDescribeCluster = 60,
  kDescribeProducers = 61,
};

inline bool IsValidAPIKey(int16_t api_key) {
  std::optional<APIKey> api_key_type_option = magic_enum::enum_cast<APIKey>(api_key);
  if (!api_key_type_option.has_value()) {
    return false;
  }
  return true;
}

constexpr int kMessageLengthBytes = 4;
constexpr int kAPIKeyLength = 2;
constexpr int kAPIVersionLength = 2;
constexpr int kCorrelationIDLength = 4;

// length, request_api_key, request_api_version, correlation_id
constexpr int kMinReqHeaderLength =
    kMessageLengthBytes + kAPIKeyLength + kAPIVersionLength + kCorrelationIDLength;
// length, correlation_id
constexpr int kMinRespHeaderLength = kMessageLengthBytes + kCorrelationIDLength;
constexpr int kMaxAPIVersion = 12;

struct Packet : public FrameBase {
  int32_t correlation_id;
  std::string msg;

  size_t ByteSize() const override { return sizeof(Packet) + msg.size(); }
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
