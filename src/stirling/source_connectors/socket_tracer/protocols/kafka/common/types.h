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

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <chrono>
#include <deque>
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
// Mapping from Kafka version to API Version
// https://cwiki.apache.org/confluence/display/KAFKA/Kafka+APIs
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

// Error Codes
// https://kafka.apache.org/protocol.html#protocol_error_codes
enum class ErrorCode : int16_t {
  kUnknownServerError = -1,
  kNone = 0,
  kOffsetOutOfRange = 1,
  kCorruptMessage = 2,
  kUnknownTopicOrPartitiov = 3,
  kInvalidFetchSize = 4,
  kLeaderNotAvailable = 5,
  kNotLeaderOrFollowee = 6,
  kRequestTimedOut = 7,
  kBrokerNotAvailable = 8,
  kReplicaNotAvailable = 9,
  kMessageTooLarge = 10,
  kStaleControllerEpoch = 11,
  kOffsetMetadataTooLarge = 12,
  kNetworkException = 13,
  kCoordinatorLoadInProgress = 14,
  kCoordinatorNotAvailable = 15,
  kNotCoordinator = 16,
  kInvalidTopicException = 17,
  kRecordListTooLarge = 18,
  kNotEnoughReplicas = 19,
  kNotEnoughReplicasAfterAppend = 20,
  kInvalidRequiredAcks = 21,
  kIllegalGeneration = 22,
  kInconsistentGroupProtocol = 23,
  kInvalidGroupID = 24,
  kUnknownMemberID = 25,
  kInvalidSessionTimeout = 26,
  kRebalanceInProgress = 27,
  kInvalidCommitOffsetSize = 28,
  kTopicAuthorizationFailed = 29,
  kGroupAuthorizationFailed = 30,
  kClusterAuthorizationFailed = 31,
  kInvalidTimestamp = 32,
  kUnsupportedSaslMechanism = 33,
  kIllegalSaslState = 34,
  kUnsupportedVersion = 35,
  kTopicAlreadyExists = 36,
  kInvalidPartitions = 37,
  kInvalidReplicationFactor = 38,
  kInvalidReplicaAssignment = 39,
  kInvalidConfig = 40,
  kNotController = 41,
  kInvalidRequest = 42,
  kUnsupportedForMessageFormat = 43,
  kPolicyViolation = 44,
  kOutOfOrderSequenceNumber = 45,
  kDuplicateSequenceNumber = 46,
  kInvalidProducerEpoch = 47,
  kInvalidTxnState = 48,
  kInvalidProducerIDMapping = 49,
  kInvalidTransactionTimeout = 50,
  kConcurrentTransactions = 51,
  kTransactionCoordinatorFenced = 52,
  kTransactionalIDAuthorizationFailed = 53,
  kSecurityDisabled = 54,
  kOperationNotAttempted = 55,
  kKafkaStorageError = 56,
  kLogDirNotFound = 57,
  kSaslAuthenticationFailed = 58,
  kUnknownProducerID = 59,
  kReassignmentInProgress = 60,
  kDelegationTokenAuthDisabled = 61,
  kDelegationTokenNotFound = 62,
  kDelegationTokenOwnerMismatch = 63,
  kDelegationTokenRequestNotAllowed = 64,
  kDelegationTokenAuthorizationFailed = 65,
  kDelegationTokenExpired = 66,
  kInvalidPrincipalType = 67,
  kNonEmptyGroup = 68,
  kGroupIDNotFound = 69,
  kFetchSessionIDNotFound = 70,
  kInvalidFetchSessionEpoch = 71,
  kListenerNotFound = 72,
  kTopicDeletionDisabled = 73,
  kFencedLeaderEpoch = 74,
  kUnknownLeaderEpoch = 75,
  kUnsupportedCompressionType = 76,
  kStaleBrokerEpoch = 77,
  kOffsetNotAvailable = 78,
  kMemberIDRequired = 79,
  kPreferredLeaderNotAvailable = 80,
  kGroupMaxSizeReached = 81,
  kFencedInstanceID = 82,
  kEligibleLeadersNotAvailable = 83,
  kElectionNotNeeded = 84,
  kNoReassignmentInProgress = 85,
  kGroupSubscribedToTopic = 86,
  kInvalidRecord = 87,
  kUnstableOffsetCommit = 88,
  kThrottlingQuotaExceeded = 89,
  kProducerFenced = 90,
  kResourceNotFound = 91,
  kDuplicateResource = 92,
  kUnacceptableCredential = 93,
  kInconsistentVoterSet = 94,
  kInvalidUpdateVersion = 95,
  kFeatureUpdateFailed = 96,
  kPrincipalDeserializationFailure = 97,
  kSnapshotNotFound = 98,
  kPositionOutOfRange = 99,
  kUnknownTopicID = 100,
  kDuplicateBrokerRegistration = 101,
  kBrokerIDNotRegistered = 102,
  kInconsistentTopicID = 103,
  kInconsistentClusterID = 104,
};

struct APIVersionData {
  int16_t kMinVersion;
  int16_t kMaxVersion;
  int16_t kflexibleVersion;
};

// A mapping of api_key to the api_versions supported and the version from which it becomes
// flexible. Flexible versions use tagged fields and more efficient serialization for
// variable-length objects.
// https://cwiki.apache.org/confluence/display/KAFKA/KIP-482%3A+The+Kafka+Protocol+should+Support+Optional+Tagged+Fields#KIP482:TheKafkaProtocolshouldSupportOptionalTaggedFields-FlexibleVersions
// Detailed information on each API key:
// https://github.com/apache/kafka/tree/trunk/clients/src/main/resources/common/message
// TODO(chengruizhe): Needs updating for new opcodes.
inline const absl::flat_hash_map<APIKey, APIVersionData> APIVersionMap = {
    // Setting min supported version to 1 to help finding frame boundary.
    {APIKey::kProduce, {1, 9, 9}},
    {APIKey::kFetch, {0, 12, 12}},
    {APIKey::kListOffsets, {0, 7, 6}},
    {APIKey::kMetadata, {0, 12, 9}},
    {APIKey::kLeaderAndIsr, {0, 5, 4}},
    {APIKey::kStopReplica, {0, 3, 2}},
    {APIKey::kUpdateMetadata, {0, 7, 6}},
    {APIKey::kControlledShutdown, {0, 3, 3}},
    {APIKey::kOffsetCommit, {0, 8, 8}},
    {APIKey::kOffsetFetch, {0, 8, 6}},
    {APIKey::kFindCoordinator, {0, 4, 3}},
    {APIKey::kJoinGroup, {0, 7, 6}},
    {APIKey::kHeartbeat, {0, 4, 4}},
    {APIKey::kLeaveGroup, {0, 4, 4}},
    {APIKey::kSyncGroup, {0, 5, 4}},
    {APIKey::kDescribeGroups, {0, 5, 5}},
    {APIKey::kListGroups, {0, 4, 3}},
    {APIKey::kSaslHandshake, {0, 1, -1}},
    {APIKey::kApiVersions, {0, 3, 3}},
    {APIKey::kCreateTopics, {0, 7, 5}},
    {APIKey::kDeleteTopics, {0, 6, 4}},
    {APIKey::kDeleteRecords, {0, 2, 2}},
    {APIKey::kInitProducerId, {0, 4, 2}},
    {APIKey::kOffsetForLeaderEpoch, {0, 4, 4}},
    {APIKey::kAddPartitionsToTxn, {0, 3, 3}},
    {APIKey::kAddOffsetsToTxn, {0, 3, 3}},
    {APIKey::kEndTxn, {0, 3, 3}},
    {APIKey::kWriteTxnMarkers, {0, 1, 1}},
    {APIKey::kTxnOffsetCommit, {0, 3, 3}},
    {APIKey::kDescribeAcls, {0, 2, 2}},
    {APIKey::kCreateAcls, {0, 2, 2}},
    {APIKey::kDeleteAcls, {0, 2, 2}},
    {APIKey::kDescribeConfigs, {0, 4, 4}},
    {APIKey::kAlterConfigs, {0, 2, 2}},
    {APIKey::kAlterReplicaLogDirs, {0, 2, 2}},
    {APIKey::kDescribeLogDirs, {0, 2, 2}},
    {APIKey::kSaslAuthenticate, {0, 2, 2}},
    {APIKey::kCreatePartitions, {0, 3, 2}},
    {APIKey::kCreateDelegationToken, {0, 2, 2}},
    {APIKey::kRenewDelegationToken, {0, 2, 2}},
    {APIKey::kExpireDelegationToken, {0, 2, 2}},
    {APIKey::kDescribeDelegationToken, {0, 2, 2}},
    {APIKey::kDeleteGroups, {0, 5, 5}},
    {APIKey::kElectLeaders, {0, 2, 2}},
    {APIKey::kIncrementalAlterConfigs, {0, 1, 1}},
    {APIKey::kAlterPartitionReassignments, {0, 0, 0}},
    {APIKey::kListPartitionReassignments, {0, 0, 0}},
    {APIKey::kOffsetDelete, {0, 0, -1}},
    {APIKey::kDescribeClientQuotas, {0, 1, 1}},
    {APIKey::kAlterClientQuotas, {0, 1, 1}},
    {APIKey::kDescribeUserScramCredentials, {0, 0, 0}},
    {APIKey::kAlterUserScramCredentials, {0, 0, 0}},
    {APIKey::kAlterIsr, {0, 0, 0}},
    {APIKey::kUpdateFeatures, {0, 0, 0}},
    {APIKey::kDescribeCluster, {0, 0, 0}},
    {APIKey::kDescribeProducers, {0, 0, 0}}};

inline bool IsFlexible(APIKey api_key, int16_t api_version) {
  auto it = APIVersionMap.find(api_key);
  if (it != APIVersionMap.end()) {
    // Negative flexible version indicates that there's no flexible version for this api key.
    if (it->second.kflexibleVersion < 0) {
      return false;
    }
    return api_version >= it->second.kflexibleVersion;
  }
  return false;
}

inline bool IsValidAPIKey(int16_t api_key) {
  std::optional<APIKey> api_key_type_option = magic_enum::enum_cast<APIKey>(api_key);
  if (!api_key_type_option.has_value()) {
    return false;
  }
  return true;
}

inline bool IsSupportedAPIVersion(APIKey api_key, int16_t api_version) {
  auto it = APIVersionMap.find(api_key);
  if (it != APIVersionMap.end()) {
    return api_version >= it->second.kMinVersion && api_version <= it->second.kMaxVersion;
  }
  return false;
}

constexpr int kMessageLengthBytes = 4;
constexpr int kAPIKeyLength = 2;
constexpr int kAPIVersionLength = 2;
constexpr int kCorrelationIDLength = 4;

// length, request_api_key, request_api_version, correlation_id
constexpr int kMinReqPacketLength =
    kMessageLengthBytes + kAPIKeyLength + kAPIVersionLength + kCorrelationIDLength;
// length, correlation_id
constexpr int kMinRespPacketLength = kMessageLengthBytes + kCorrelationIDLength;
constexpr int kMaxAPIVersion = 12;

struct Packet : public FrameBase {
  int32_t correlation_id;
  std::string msg;
  // `consumed` is used to mark if a request packet has been matched to a response in StitchFrames.
  // This is an optimization to efficiently remove all matched packets from the front of the deque.
  bool consumed = false;

  size_t ByteSize() const override { return sizeof(Packet) + msg.size(); }
};

struct Request {
  // Kafka opcode.
  APIKey api_key;

  // Version of the Kafka API Key.
  int16_t api_version;

  // Client ID present in request api version >= 1.
  std::string client_id;

  // Request message.
  std::string msg;

  uint64_t timestamp_ns;

  std::string ToString() const {
    return absl::Substitute("timestamp=$0 client_id=$1 api_key=$2(version: $3) msg=$4",
                            timestamp_ns, client_id, magic_enum::enum_name(api_key), api_version,
                            msg);
  }
};

struct Response {
  // Response message.
  std::string msg;

  uint64_t timestamp_ns;

  std::string ToString() const {
    return absl::Substitute("timestamp=$0 msg=$1", timestamp_ns, msg);
  }
};

struct Record {
  Request req;
  Response resp;

  // Debug information.
  std::string px_info = "";

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

// seen_correlation_ids is the correlation_ids of the received requests. It can be used to
// more robustly implement FindFrameBoundary for Kafka response packets.
struct State {
  absl::flat_hash_set<int32_t> seen_correlation_ids;
};

struct StateWrapper {
  State global;
  std::monostate send;
  std::monostate recv;
};

using correlation_id_t = uint16_t;
struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Packet;
  using record_type = Record;
  using state_type = StateWrapper;
  using key_type = correlation_id_t;
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
