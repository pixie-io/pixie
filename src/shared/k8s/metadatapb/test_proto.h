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
)";

const char* kToBeTerminatedPodUpdatePbTxt = R"(
uid: "2_uid"
name: "terminating_pod"
namespace: "pl"
start_timestamp_ns: 10
container_ids: "pod2_container_1"
qos_class: QOS_CLASS_BEST_EFFORT
node_name: "test_node_tbt"
hostname: "test_host_tbt"
phase: FAILED
message: "Failed message unterminated"
reason: "Failed reason unterminated"
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

std::unique_ptr<px::shared::k8s::metadatapb::ResourceUpdate> CreateRunningPodUpdatePB() {
  auto update = std::make_unique<px::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto = absl::Substitute(kResourceUpdateTmpl, "pod_update", kRunningPodUpdatePbTxt);
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

}  // namespace testutils
}  // namespace metadatapb
}  // namespace px
