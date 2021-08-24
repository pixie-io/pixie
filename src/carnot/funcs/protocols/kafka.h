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

#include <string>

namespace px {
namespace carnot {
namespace funcs {
namespace protocols {
namespace kafka {

inline std::string APIKeyName(int api_key) {
  switch (api_key) {
    case 0:
      return "Produce";
    case 1:
      return "Fetch";
    case 2:
      return "ListOffsets";
    case 3:
      return "Metadata";
    case 4:
      return "LeaderAndIsr";
    case 5:
      return "StopReplica";
    case 6:
      return "UpdateMetadata";
    case 7:
      return "ControlledShutdown";
    case 8:
      return "OffsetCommit";
    case 9:
      return "OffsetFetch";
    case 10:
      return "FindCoordinator";
    case 11:
      return "JoinGroup";
    case 12:
      return "Heartbeat";
    case 13:
      return "LeaveGroup";
    case 14:
      return "SyncGroup";
    case 15:
      return "DescribeGroups";
    case 16:
      return "ListGroups";
    case 17:
      return "SaslHandshake";
    case 18:
      return "ApiVersions";
    case 19:
      return "CreateTopics";
    case 20:
      return "DeleteTopics";
    case 21:
      return "DeleteRecords";
    case 22:
      return "InitProducerId";
    case 23:
      return "OffsetForLeaderEpoch";
    case 24:
      return "AddPartitionsToTxn";
    case 25:
      return "AddOffsetsToTxn";
    case 26:
      return "EndTxn";
    case 27:
      return "WriteTxnMarkers";
    case 28:
      return "TxnOffsetCommit";
    case 29:
      return "DescribeAcls";
    case 30:
      return "CreateAcls";
    case 31:
      return "DeleteAcls";
    case 32:
      return "DescribeConfigs";
    case 33:
      return "AlterConfigs";
    case 34:
      return "AlterReplicaLogDirs";
    case 35:
      return "DescribeLogDirs";
    case 36:
      return "SaslAuthenticate";
    case 37:
      return "CreatePartitions";
    case 38:
      return "CreateDelegationToken";
    case 39:
      return "RenewDelegationToken";
    case 40:
      return "ExpireDelegationToken";
    case 41:
      return "DescribeDelegationToken";
    case 42:
      return "DeleteGroups";
    case 43:
      return "ElectLeaders";
    case 44:
      return "IncrementalAlterConfigs";
    case 45:
      return "AlterPartitionReassignments";
    case 46:
      return "ListPartitionReassignments";
    case 47:
      return "OffsetDelete";
    case 48:
      return "DescribeClientQuotas";
    case 49:
      return "AlterClientQuotas";
    case 50:
      return "DescribeUserScramCredentials";
    case 51:
      return "AlterUserScramCredentials";
    case 56:
      return "AlterIsr";
    case 57:
      return "UpdateFeatures";
    case 60:
      return "DescribeCluster";
    case 61:
      return "DescribeProducers";
    default:
      return std::to_string(api_key);
  }
}

}  // namespace kafka

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
