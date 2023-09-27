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

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/substitute.h>
#include "src/common/base/base.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"

namespace px {
namespace metadatapb {
namespace testutils {
/*
 * Template for a resource update.
 */
const char* kResourceUpdateTmpl = R"(
$0 {
  $1
}
)";

/*
 *  Templates for pod updates.
 */
const char* kRunningPodUpdatePbTxt = R"(
uid: "1_uid"
name: "running_pod"
namespace: "pl"
labels: "{\"k1\":\"v1\", \"k2\":\"v2\"}"
start_timestamp_ns: 5
container_ids: "pod1_container_1"
qos_class: QOS_CLASS_GUARANTEED
node_name: "test_node"
hostname: "test_host"
pod_ip: "1.1.1.1"
phase: RUNNING
message: "Running message"
reason: "Running reason"
conditions {
  type: 2
  status: 1
}
owner_references: {
  uid: "rs0_uid"
  name: "rs0"
  kind: "ReplicaSet"
}
)";

const char* kReusedIPPodUpdatePbTxt = R"(
uid: "101_uid"
name: "reused_ip"
namespace: "pl"
start_timestamp_ns: 100
container_ids: "pod1_container_1"
qos_class: QOS_CLASS_GUARANTEED
node_name: "test_node"
hostname: "test_host"
pod_ip: "1.1.1.1"
phase: RUNNING
message: "Running message"
reason: "Running reason"
conditions {
  type: 2
  status: 1
}
)";

const char* kToBeTerminatedPodUpdatePbTxt = R"(
uid: "2_uid"
name: "terminating_pod"
namespace: "pl"
labels: "{\"k1\":\"v1\"}"
start_timestamp_ns: 10
container_ids: "pod2_container_1"
qos_class: QOS_CLASS_BEST_EFFORT
node_name: "test_node_tbt"
hostname: "test_host_tbt"
phase: FAILED
message: "Failed message unterminated"
reason: "Failed reason unterminated"
owner_references: {
  uid: "terminating_rs0_uid"
  name: "terminating_rs"
  kind: "ReplicaSet"
}
)";

const char* kTerminatedPodUpdatePbTxt = R"(
uid: "2_uid"
name: "terminating_pod"
namespace: "pl"
start_timestamp_ns: 10
stop_timestamp_ns: 15
container_ids: "pod2_container_1"
qos_class: QOS_CLASS_BEST_EFFORT
phase: FAILED
message: "Failed message terminated"
reason: "Failed reason terminated"
conditions {
  type: 2
  status: 2
}
owner_references: {
  uid: "terminating_rs0_uid"
  name: "terminating_rs"
  kind: "ReplicaSet"
}
)";

const char* kPodMissingOwnerPbTxt = R"(
uid: "missing_owner_3_uid"
name: "missing_owner_pod"
namespace: "pl"
start_timestamp_ns: 5
container_ids: "pod2_container_1"
qos_class: QOS_CLASS_GUARANTEED
node_name: "test_node"
hostname: "test_host"
pod_ip: "4.4.4.4"
phase: RUNNING
message: "Running message"
reason: "Running reason"
conditions {
  type: 2
  status: 1
}
owner_references: {
  uid: "rs_missing_uid"
  name: "rs0"
  kind: "ReplicaSet"
}
)";

/*
 * Templates for container updates.
 */
const char* kRunningContainerUpdatePbTxt = R"(
cid: "pod1_container_1"
name: "running_container"
start_timestamp_ns: 6
container_state: CONTAINER_STATE_RUNNING
message: "Running message"
reason: "Running reason"
)";

const char* kTerminatingContainerUpdatePbTxt = R"(
cid: "pod2_container_1"
name: "terminating_container"
start_timestamp_ns: 7
container_state: CONTAINER_STATE_TERMINATED
message: "Terminating message pending"
reason: "Terminating reason pending"
)";

const char* kTerminatedContainerUpdatePbTxt = R"(
cid: "pod2_container_1"
name: "terminating_container"
start_timestamp_ns: 7
stop_timestamp_ns: 14
container_state: CONTAINER_STATE_TERMINATED
message: "Terminating message complete"
reason: "Terminating reason complete"
)";

/*
 *  Templates for service updates.
 */
const char* kRunningServiceUpdatePbTxt = R"(
uid: "3_uid"
name: "running_service"
namespace: "pl"
start_timestamp_ns: 7
pod_ids: "1_uid"
)";

const char* kRunningServiceIPUpdatePbTxt = R"(
uid: "3_uid"
name: "running_service"
namespace: "pl"
external_ips: "127.0.0.1"
cluster_ip: "127.0.0.2"
)";

const char* kToBeTerminatedServiceUpdatePbTxt = R"(
uid: "4_uid"
name: "terminating_service"
namespace: "pl"
start_timestamp_ns: 7
pod_ids: "2_uid"
)";

const char* kTerminatedServiceUpdatePbTxt = R"(
uid: "4_uid"
name: "terminating_service"
namespace: "pl"
start_timestamp_ns: 7
stop_timestamp_ns: 20
pod_ids: "2_uid"
)";

// Represents a separate service that points
const char* kServiceWithDuplicatePodUpdatePbTxt = R"(
uid: "5_uid"
name: "other_service_with_pod"
namespace: "pl"
start_timestamp_ns: 7
pod_ids: "1_uid"
)";

/*
 * Templates for replica set updates.
 */
const char* kRunningReplicaSetUpdatePbTxt = R"(
uid: "rs0_uid"
name: "rs0"
start_timestamp_ns: 101
stop_timestamp_ns: 0
namespace: "pl"
replicas: 5
fully_labeled_replicas: 5
ready_replicas: 3
available_replicas: 3
observed_generation: 5
requested_replicas: 5
conditions: {
  type: "ready"
  status: 1
}
owner_references: {
  kind: "Deployment"
  name: "deployment1"
  uid: "deployment_uid"
}
)";

const char* kTerminatingReplicaSetUpdatePbTxt = R"(
uid: "terminating_rs0_uid"
name: "terminating_rs"
namespace: "pl"
replicas: 0
fully_labeled_replicas: 0
ready_replicas: 0
available_replicas: 0
observed_generation: 5
requested_replicas: 0
start_timestamp_ns: 101
conditions: {
  type: "Terminating"
  status: 1
}
owner_references: {
  kind: "Deployment"
  name: "terminating_deployment1"
  uid: "terminating_deployment_uid"
}
)";

const char* kTerminatedReplicaSetUpdatePbTxt = R"(
uid: "terminating_rs0_uid"
name: "terminating_rs"
namespace: "pl"
start_timestamp_ns: 101
stop_timestamp_ns: 150
replicas: 0
fully_labeled_replicas: 0
ready_replicas: 0
available_replicas: 0
requested_replicas: 0
observed_generation: 5
conditions: {
  type: "Terminating"
  status: 1
}
owner_references: {
  kind: "Deployment"
  name: "terminating_deployment1"
  uid: "terminating_deployment_uid"
}
)";

/*
 * Templates for deployment updates.
 */
const char* kRunningDeploymentUpdatePbTxt = R"(
uid: "deployment_uid"
name: "deployment1"
start_timestamp_ns: 101
stop_timestamp_ns: 0
namespace: "pl"
replicas: 5
updated_replicas: 4
ready_replicas: 3
available_replicas: 3
unavailable_replicas: 2
requested_replicas: 5
observed_generation: 5
conditions: {
  type: 1
  status: CONDITION_STATUS_TRUE
}
)";

const char* kTerminatingDeploymentUpdatePbTxt = R"(
uid: "terminating_deployment_uid"
name: "terminating_deployment1"
namespace: "pl"
start_timestamp_ns: 123
stop_timestamp_ns: 0
replicas: 6
updated_replicas: 5
ready_replicas: 3
available_replicas: 3
unavailable_replicas: 2
requested_replicas: 0
observed_generation: 2
conditions: {
  type: 1
  status: CONDITION_STATUS_FALSE
}
)";

const char* kTerminatedDeploymentUpdatePbTxt = R"(
uid: "terminating_deployment_uid"
name: "terminating_deployment1"
namespace: "pl"
start_timestamp_ns: 123
stop_timestamp_ns: 150
replicas: 0
updated_replicas: 0
ready_replicas: 0
available_replicas: 0
unavailable_replicas: 0
requested_replicas: 0
observed_generation: 2
conditions: {
  type: 1
  status: CONDITION_STATUS_FALSE
}
)";

/*
 * Templates for namespace updates.
 */
const char* kRunningNamespaceUpdatePbTxt = R"(
uid: "namespace_uid"
name: "namespace1"
start_timestamp_ns: 101
stop_timestamp_ns: 0
)";

const char* kTerminatingNamespaceUpdatePbTxt = R"(
uid: "terminating_namespace_uid"
name: "terminating_namespace1"
start_timestamp_ns: 123
stop_timestamp_ns: 0
)";

const char* kTerminatedNamespaceUpdatePbTxt = R"(
uid: "terminating_namespace_uid"
name: "terminating_namespace1"
start_timestamp_ns: 123
stop_timestamp_ns: 150
)";

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateRunningPodUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto = absl::Substitute(kResourceUpdateTmpl, "pod_update", kRunningPodUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateReusedIPUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto = absl::Substitute(kResourceUpdateTmpl, "pod_update", kReusedIPPodUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateMissingOwnerPodUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto = absl::Substitute(kResourceUpdateTmpl, "pod_update", kPodMissingOwnerPbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatingPodUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "pod_update", kToBeTerminatedPodUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatedPodUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "pod_update", kTerminatedPodUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateRunningContainerUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "container_update", kRunningContainerUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatingContainerUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "container_update", kTerminatingContainerUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatedContainerUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "container_update", kTerminatedContainerUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateRunningServiceUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "service_update", kRunningServiceUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateRunningServiceIPUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "service_update", kRunningServiceIPUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatingServiceUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "service_update", kToBeTerminatedServiceUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatedServiceUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "service_update", kTerminatedServiceUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateServiceWithSamePodUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "service_update", kServiceWithDuplicatePodUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateRunningReplicaSetUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "replica_set_update", kRunningReplicaSetUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatingReplicaSetUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto = absl::Substitute(kResourceUpdateTmpl, "replica_set_update",
                                       kTerminatingReplicaSetUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatedReplicaSetUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "replica_set_update", kTerminatedReplicaSetUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateRunningDeploymentUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "deployment_update", kRunningDeploymentUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatingDeploymentUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "deployment_update", kTerminatingDeploymentUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatedDeploymentUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "deployment_update", kTerminatedDeploymentUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateRunningNamespaceUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "namespace_update", kRunningNamespaceUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatingNamespaceUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "namespace_update", kTerminatingNamespaceUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatedNamespaceUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "namespace_update", kTerminatedNamespaceUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

}  // namespace testutils
}  // namespace metadatapb
}  // namespace px
