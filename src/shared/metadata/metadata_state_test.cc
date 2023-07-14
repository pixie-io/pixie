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

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/common/testing/testing.h"
#include "src/shared/metadata/metadata_state.h"

namespace px {
namespace md {

using ::google::protobuf::TextFormat;
using ::testing::UnorderedElementsAre;

constexpr char kPod0UpdatePbTxt[] = R"(
  uid: "pod0_uid"
  name: "pod0"
  namespace: "ns0"
  labels: "{\"k1\":\"v1\", \"k2\":\"v2\"}"
  start_timestamp_ns: 101
  stop_timestamp_ns: 103
  container_ids: "container0_uid"
  container_ids: "container1_uid"
  container_names: "container0"
  container_names: "container1"
  qos_class: QOS_CLASS_GUARANTEED
  phase: RUNNING
  conditions: {
    type: READY
    status: CONDITION_STATUS_TRUE
  }
  owner_references: {
    uid: "1234"
    kind: "ReplicaSet"
    name: "rs1"
  }
  node_name: "a_node"
  hostname: "a_host"
  pod_ip: "1.2.3.4"
  host_ip: "5.6.7.8"
  message: "a pod message"
  reason: "a pod reason"
)";

constexpr char kPod1UpdatePbTxt[] = R"(
  uid: "pod1_uid"
  name: "pod1"
  namespace: "ns0"
  labels: "{\"k1\":\"v1\"}"
  start_timestamp_ns: 101
  stop_timestamp_ns: 103
  container_ids: "container0_uid"
  container_ids: "container1_uid"
  container_names: "container0"
  container_names: "container1"
  qos_class: QOS_CLASS_GUARANTEED
  phase: TERMINATED
  conditions: {
    type: READY
    status: CONDITION_STATUS_TRUE
  }
  node_name: "a_node"
  hostname: "a_host"
  pod_ip: "1.2.3.5"
  host_ip: "5.6.7.8"
  message: "a pod message"
  reason: "a pod reason"
)";

constexpr char kPod2UpdatePbTxt[] = R"(
  uid: "pod2_uid"
  name: "pod2"
  namespace: "ns0"
  labels: "{\"k1\":\"v1\"}"
  start_timestamp_ns: 107
  stop_timestamp_ns: 0
  container_ids: "container0_uid"
  container_ids: "container1_uid"
  container_names: "container0"
  container_names: "container1"
  qos_class: QOS_CLASS_GUARANTEED
  phase: RUNNING
  conditions: {
    type: READY
    status: CONDITION_STATUS_TRUE
  }
  node_name: "a_node"
  hostname: "a_host"
  pod_ip: "1.2.3.5"
  host_ip: "5.6.7.8"
  message: "a pod message"
  reason: "a pod reason"
)";

constexpr char kContainer0UpdatePbTxt[] = R"(
  cid: "container0_uid"
  name: "container0"
  namespace: "ns0"
  start_timestamp_ns: 100
  stop_timestamp_ns: 102
  pod_id: "pod0_uid"
  pod_name: "pod0"
  container_state: CONTAINER_STATE_RUNNING
  message: "a container message"
  reason: "a container reason"
)";

constexpr char kRunningServiceUpdatePbTxt[] = R"(
  uid: "service0_uid"
  name: "running_service"
  namespace: "ns0"
  start_timestamp_ns: 7
  stop_timestamp_ns: 8
  pod_ids: "pod0_uid"
  pod_ids: "pod1_uid"
  pod_names: "pod0"
  pod_names: "pod1"
)";

constexpr char kRunningServiceUpdateExternalIPsPbTxt[] = R"(
  uid: "service0_uid"
  name: "running_service"
  namespace: "ns0"
  external_ips: "127.0.0.1"
  cluster_ip: "127.0.0.2"
)";

constexpr char kRunningNamespaceUpdatePbTxt[] = R"(
  uid: "ns0_uid"
  name: "ns0"
  start_timestamp_ns: 7
  stop_timestamp_ns: 8
)";

constexpr char kReplicaSetUpdatePbTxt[] = R"(
  uid: "rs0_uid"
  name: "rs0"
  start_timestamp_ns: 101
  stop_timestamp_ns: 0
  namespace: "ns0"
  replicas: 5
  fully_labeled_replicas: 5
  ready_replicas: 3
  available_replicas: 3
  observed_generation: 5
  requested_replicas: 5
  conditions: {
    type: "ready"
    status: CONDITION_STATUS_TRUE
  }
  owner_references: {
    kind: "Deployment"
    name: "deployment1"
    uid: "deployment_uid"
  }
)";

constexpr char kReplicaSetUpdatePbTxt00[] = R"(
  uid: "rs0_uid"
  name: "rs0"
  start_timestamp_ns: 101
  stop_timestamp_ns: 1
  namespace: "ns0"
  replicas: 5
  fully_labeled_replicas: 5
  ready_replicas: 3
  available_replicas: 3
  observed_generation: 5
  requested_replicas: 5
  conditions: {
    type: "ready"
    status: CONDITION_STATUS_TRUE
  }
  owner_references: {
    kind: "Deployment"
    name: "deployment1"
    uid: "deployment_uid"
  }
)";

constexpr char kDeploymentUpdatePbTxt00[] = R"(
  uid: "deployment_uid"
  name: "deployment1"
  start_timestamp_ns: 101
  stop_timestamp_ns: 0
  namespace: "ns0"
  replicas: 5
  updated_replicas: 5
  ready_replicas: 3
  available_replicas: 3
  unavailable_replicas: 2
  observed_generation: 5
  requested_replicas: 5
  conditions: {
    type: 1
    status: CONDITION_STATUS_TRUE
  }
  conditions: {
    type: 2
    status: CONDITION_STATUS_TRUE
  }
)";

constexpr char kDeploymentUpdatePbTxt01[] = R"(
  uid: "deployment_uid"
  name: "deployment1"
  start_timestamp_ns: 101
  stop_timestamp_ns: 0
  namespace: "ns0"
  replicas: 6
  updated_replicas: 4
  ready_replicas: 2
  available_replicas: 2
  unavailable_replicas: 4
  observed_generation: 6
  requested_replicas: 6
  conditions: {
    type: 1
    status: CONDITION_STATUS_TRUE
  }
  conditions: {
    type: 2
    status: CONDITION_STATUS_FALSE
  }
  conditions: {
    type: 3
    status: CONDITION_STATUS_TRUE
  }
)";

constexpr char kDeploymentUpdatePbTxt02[] = R"(
  uid: "deployment_uid_2"
  name: "deployment2"
  start_timestamp_ns: 110
  stop_timestamp_ns: 200
  namespace: "ns1"
  replicas: 10
  updated_replicas: 6
  ready_replicas: 1
  available_replicas: 0
  unavailable_replicas: 10
  observed_generation: 10
  requested_replicas: 7

  conditions: {
    type: 1
    status: CONDITION_STATUS_FALSE
  }
  conditions: {
    type: 2
    status: CONDITION_STATUS_FALSE
  }
  conditions: {
    type: 3
    status: CONDITION_STATUS_TRUE
  }
)";

constexpr char kDeploymentUpdatePbTxt03[] = R"(
  uid: "deployment_uid"
  name: "deployment1"
  start_timestamp_ns: 101
  stop_timestamp_ns: 1
  namespace: "ns0"
  replicas: 5
  updated_replicas: 5
  ready_replicas: 3
  available_replicas: 3
  unavailable_replicas: 2
  observed_generation: 5
  requested_replicas: 5
  conditions: {
    type: 1
    status: CONDITION_STATUS_TRUE
  }
  conditions: {
    type: 2
    status: CONDITION_STATUS_TRUE
  }
)";

TEST(K8sMetadataStateTest, CloneCopiedCIDR) {
  K8sMetadataState state;

  CIDRBlock pod_cidr0;
  CIDRBlock pod_cidr1;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/10", &pod_cidr0));
  ASSERT_OK(ParseCIDRBlock("16.17.18.19/10", &pod_cidr1));
  std::vector<CIDRBlock> pod_cidrs = {pod_cidr0, pod_cidr1};
  state.set_pod_cidrs(pod_cidrs);

  CIDRBlock service_cidr;
  ASSERT_OK(ParseCIDRBlock("10.64.0.0/16", &service_cidr));
  state.set_service_cidr(service_cidr);

  auto state_copy = state.Clone();

  auto& state_copy_pod_cidrs = state_copy->pod_cidrs();
  ASSERT_EQ(state_copy_pod_cidrs.size(), 2);
  EXPECT_EQ(pod_cidrs[0].ip_addr.AddrStr(), state_copy_pod_cidrs[0].ip_addr.AddrStr());
  EXPECT_EQ(pod_cidrs[0].prefix_length, state_copy_pod_cidrs[0].prefix_length);
  EXPECT_EQ(pod_cidrs[1].ip_addr.AddrStr(), state_copy_pod_cidrs[1].ip_addr.AddrStr());
  EXPECT_EQ(pod_cidrs[1].prefix_length, state_copy_pod_cidrs[1].prefix_length);

  ASSERT_TRUE(state_copy->service_cidr().has_value());
  EXPECT_EQ(service_cidr.ip_addr.AddrStr(), state_copy->service_cidr()->ip_addr.AddrStr());
  EXPECT_EQ(service_cidr.prefix_length, state_copy->service_cidr()->prefix_length);
}

TEST(K8sMetadataStateTest, HandleContainerUpdate) {
  K8sMetadataState state;

  K8sMetadataState::ContainerUpdate update;
  ASSERT_TRUE(TextFormat::MergeFromString(kContainer0UpdatePbTxt, &update))
      << "Failed to parse proto";

  EXPECT_OK(state.HandleContainerUpdate(update));
  auto info = state.ContainerInfoByID("container0_uid");
  ASSERT_NE(nullptr, info);
  EXPECT_EQ("container0_uid", info->cid());
  EXPECT_EQ("container0", info->name());
  // Shouldn't be set until the pod update.
  EXPECT_EQ("", info->pod_id());
  EXPECT_EQ(100, info->start_time_ns());
  EXPECT_EQ(102, info->stop_time_ns());
  EXPECT_EQ(ContainerState::kRunning, info->state());
  EXPECT_EQ("a container message", info->state_message());
  EXPECT_EQ("a container reason", info->state_reason());
}

TEST(K8sMetadataStateTest, HandlePodUpdate) {
  // 1 missing ContainerUpdate (should be skipped).
  // 1 present ContainerUpdate (should be handled before PodUpdate).
  K8sMetadataState state;

  K8sMetadataState::ContainerUpdate container_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kContainer0UpdatePbTxt, &container_update))
      << "Failed to parse proto";

  K8sMetadataState::PodUpdate pod_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kPod0UpdatePbTxt, &pod_update))
      << "Failed to parse proto";

  // We expect container updates will be run before the pod update.
  EXPECT_OK(state.HandleContainerUpdate(container_update));

  auto container_info = state.ContainerInfoByID("container0_uid");
  ASSERT_NE(nullptr, container_info);
  EXPECT_EQ("", container_info->pod_id());

  EXPECT_OK(state.HandlePodUpdate(pod_update));

  auto pod_info = state.PodInfoByID("pod0_uid");
  ASSERT_NE(nullptr, pod_info);
  EXPECT_EQ("pod0_uid", pod_info->uid());
  EXPECT_EQ("pod0", pod_info->name());
  EXPECT_EQ("ns0", pod_info->ns());
  EXPECT_EQ("{\"k1\":\"v1\", \"k2\":\"v2\"}", pod_info->labels());
  EXPECT_EQ(PodQOSClass::kGuaranteed, pod_info->qos_class());
  EXPECT_EQ(PodPhase::kRunning, pod_info->phase());
  EXPECT_EQ(1, pod_info->conditions().size());
  EXPECT_EQ(ConditionStatus::kTrue, pod_info->conditions()[PodConditionType::kReady]);
  EXPECT_EQ(PodQOSClass::kGuaranteed, pod_info->qos_class());
  EXPECT_EQ(101, pod_info->start_time_ns());
  EXPECT_EQ(103, pod_info->stop_time_ns());
  EXPECT_EQ("a pod message", pod_info->phase_message());
  EXPECT_EQ("a pod reason", pod_info->phase_reason());
  EXPECT_EQ("a_node", pod_info->node_name());
  EXPECT_EQ("a_host", pod_info->hostname());
  EXPECT_EQ("1.2.3.4", pod_info->pod_ip());
  EXPECT_THAT(pod_info->containers(), UnorderedElementsAre("container0_uid"));
  EXPECT_THAT(pod_info->owner_references(),
              UnorderedElementsAre(OwnerReference{"1234", "rs0", "ReplicaSet"}));

  // Check that the container info pod ID got set.
  EXPECT_EQ("pod0_uid", container_info->pod_id());
}

TEST(K8sMetadataStateTest, HandleServiceUpdate) {
  // 1 missing PodUpdate (should be skipped).
  // 1 present PodUpdate (should be handled before ServiceUpdate).
  K8sMetadataState state;

  K8sMetadataState::PodUpdate pod_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kPod0UpdatePbTxt, &pod_update))
      << "Failed to parse proto";

  K8sMetadataState::ServiceUpdate service_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kRunningServiceUpdatePbTxt, &service_update))
      << "Failed to parse proto";

  K8sMetadataState::ServiceUpdate service_ip_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kRunningServiceUpdateExternalIPsPbTxt, &service_update))
      << "Failed to parse proto";

  EXPECT_OK(state.HandlePodUpdate(pod_update));

  auto pod_info = state.PodInfoByID("pod0_uid");
  ASSERT_NE(nullptr, pod_info);
  EXPECT_EQ(0, pod_info->services().size());

  EXPECT_OK(state.HandleServiceUpdate(service_update));

  auto service_info = state.ServiceInfoByID("service0_uid");
  ASSERT_NE(nullptr, service_info);
  EXPECT_EQ("service0_uid", service_info->uid());
  EXPECT_EQ("running_service", service_info->name());
  EXPECT_EQ("ns0", service_info->ns());
  EXPECT_EQ(7, service_info->start_time_ns());
  EXPECT_EQ(8, service_info->stop_time_ns());

  // Check that the pod info service got set.
  EXPECT_THAT(pod_info->services(), UnorderedElementsAre("service0_uid"));

  EXPECT_OK(state.HandleServiceUpdate(service_ip_update));

  // Existing metadata shouldn't be overwritten in this update when those fields
  // aren't filled out by the subsequent update.
  EXPECT_EQ("service0_uid", service_info->uid());
  EXPECT_EQ("running_service", service_info->name());
  EXPECT_EQ("ns0", service_info->ns());
  EXPECT_EQ(7, service_info->start_time_ns());
  EXPECT_EQ(8, service_info->stop_time_ns());
  EXPECT_EQ(std::vector<std::string>{"127.0.0.1"}, service_info->external_ips());
  EXPECT_EQ("127.0.0.2", service_info->cluster_ip());

  // Ditto for the other way around.
  EXPECT_OK(state.HandleServiceUpdate(service_update));
  EXPECT_EQ(std::vector<std::string>{"127.0.0.1"}, service_info->external_ips());
  EXPECT_EQ("127.0.0.2", service_info->cluster_ip());
}

TEST(K8sMetadataStateTest, HandleNamespaceUpdate) {
  K8sMetadataState state;

  K8sMetadataState::NamespaceUpdate update;
  ASSERT_TRUE(TextFormat::MergeFromString(kRunningNamespaceUpdatePbTxt, &update))
      << "Failed to parse proto";

  EXPECT_OK(state.HandleNamespaceUpdate(update));
  auto info = state.NamespaceInfoByID("ns0_uid");
  ASSERT_NE(nullptr, info);
  EXPECT_EQ("ns0_uid", info->uid());
  EXPECT_EQ("ns0", info->name());
  EXPECT_EQ(7, info->start_time_ns());
  EXPECT_EQ(8, info->stop_time_ns());
}

TEST(K8sMetadataStateTest, HandleReplicaSetUpdate) {
  K8sMetadataState state;

  K8sMetadataState::ReplicaSetUpdate update;
  ASSERT_TRUE(TextFormat::MergeFromString(kReplicaSetUpdatePbTxt, &update))
      << "Failed to parse proto";

  EXPECT_OK(state.HandleReplicaSetUpdate(update));
  auto info = state.ReplicaSetInfoByID("rs0_uid");
  ASSERT_NE(nullptr, info);
  EXPECT_EQ("rs0_uid", info->uid());
  EXPECT_EQ("rs0", info->name());
  EXPECT_EQ(101, info->start_time_ns());
  EXPECT_EQ(0, info->stop_time_ns());
  EXPECT_EQ(5, info->replicas());
  EXPECT_EQ(5, info->fully_labeled_replicas());
  EXPECT_EQ(3, info->ready_replicas());
  EXPECT_EQ(3, info->available_replicas());
  EXPECT_EQ(5, info->observed_generation());
  EXPECT_EQ(5, info->requested_replicas());
}

TEST(K8sMetadataStateTest, HandleDeploymentUpdate) {
  K8sMetadataState state;

  K8sMetadataState::DeploymentUpdate update01;
  ASSERT_TRUE(TextFormat::MergeFromString(kDeploymentUpdatePbTxt00, &update01))
      << "Failed to parse proto";

  auto info = state.DeploymentInfoByID("deployment_uid");
  ASSERT_EQ(nullptr, info);
  EXPECT_OK(state.HandleDeploymentUpdate(update01));
  info = state.DeploymentInfoByID("deployment_uid");
  ASSERT_NE(nullptr, info);
  EXPECT_EQ("deployment_uid", info->uid());
  EXPECT_EQ("deployment1", info->name());
  EXPECT_EQ("ns0", info->ns());
  EXPECT_EQ(5, info->replicas());
  EXPECT_EQ(5, info->updated_replicas());
  EXPECT_EQ(3, info->ready_replicas());
  EXPECT_EQ(3, info->available_replicas());
  EXPECT_EQ(2, info->unavailable_replicas());
  EXPECT_EQ(5, info->observed_generation());
  EXPECT_EQ(5, info->observed_generation());
  EXPECT_EQ(101, info->start_time_ns());
  EXPECT_EQ(0, info->stop_time_ns());
  EXPECT_EQ(2, info->conditions().size());
  EXPECT_EQ(ConditionStatus::kTrue, info->conditions()[DeploymentConditionType::kAvailable]);
  EXPECT_EQ(ConditionStatus::kTrue, info->conditions()[DeploymentConditionType::kProgressing]);

  // check that all fields update when another update comes in
  K8sMetadataState::DeploymentUpdate update02;
  ASSERT_TRUE(TextFormat::MergeFromString(kDeploymentUpdatePbTxt01, &update02))
      << "Failed to parse proto";
  EXPECT_OK(state.HandleDeploymentUpdate(update02));
  info = state.DeploymentInfoByID("deployment_uid");
  ASSERT_NE(nullptr, info);
  EXPECT_EQ("deployment_uid", info->uid());
  EXPECT_EQ("deployment1", info->name());
  EXPECT_EQ("ns0", info->ns());
  EXPECT_EQ(6, info->replicas());
  EXPECT_EQ(4, info->updated_replicas());
  EXPECT_EQ(2, info->ready_replicas());
  EXPECT_EQ(2, info->available_replicas());
  EXPECT_EQ(4, info->unavailable_replicas());
  EXPECT_EQ(6, info->observed_generation());
  EXPECT_EQ(6, info->observed_generation());
  EXPECT_EQ(101, info->start_time_ns());
  EXPECT_EQ(0, info->stop_time_ns());
  EXPECT_EQ(3, info->conditions().size());
  EXPECT_EQ(ConditionStatus::kTrue, info->conditions()[DeploymentConditionType::kAvailable]);
  EXPECT_EQ(ConditionStatus::kFalse, info->conditions()[DeploymentConditionType::kProgressing]);
  EXPECT_EQ(ConditionStatus::kTrue, info->conditions()[DeploymentConditionType::kReplicaFailure]);

  // //check that a new update updates a different info object
  info = state.DeploymentInfoByID("deployment_uid_2");
  ASSERT_EQ(nullptr, info);

  K8sMetadataState::DeploymentUpdate update03;
  ASSERT_TRUE(TextFormat::MergeFromString(kDeploymentUpdatePbTxt02, &update03))
      << "Failed to parse proto";
  EXPECT_OK(state.HandleDeploymentUpdate(update03));
  info = state.DeploymentInfoByID("deployment_uid_2");
  ASSERT_NE(nullptr, info);
  EXPECT_EQ("deployment_uid_2", info->uid());
  EXPECT_EQ("deployment2", info->name());
  EXPECT_EQ("ns1", info->ns());
  EXPECT_EQ(10, info->replicas());
  EXPECT_EQ(6, info->updated_replicas());
  EXPECT_EQ(1, info->ready_replicas());
  EXPECT_EQ(0, info->available_replicas());
  EXPECT_EQ(10, info->unavailable_replicas());
  EXPECT_EQ(10, info->observed_generation());
  EXPECT_EQ(7, info->requested_replicas());
  EXPECT_EQ(110, info->start_time_ns());
  EXPECT_EQ(200, info->stop_time_ns());
  EXPECT_EQ(3, info->conditions().size());
  EXPECT_EQ(ConditionStatus::kFalse, info->conditions()[DeploymentConditionType::kAvailable]);
  EXPECT_EQ(ConditionStatus::kFalse, info->conditions()[DeploymentConditionType::kProgressing]);
  EXPECT_EQ(ConditionStatus::kTrue, info->conditions()[DeploymentConditionType::kReplicaFailure]);
}

TEST(K8sMetadataStateTest, ReusedIPs) {
  K8sMetadataState state;

  K8sMetadataState::PodUpdate pod_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kPod0UpdatePbTxt, &pod_update));

  K8sMetadataState::PodUpdate terminated_ip_pod_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kPod1UpdatePbTxt, &terminated_ip_pod_update));

  K8sMetadataState::PodUpdate running_ip_pod_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kPod2UpdatePbTxt, &running_ip_pod_update));

  EXPECT_OK(state.HandlePodUpdate(pod_update));
  EXPECT_OK(state.HandlePodUpdate(terminated_ip_pod_update));
  EXPECT_OK(state.HandlePodUpdate(running_ip_pod_update));

  ASSERT_EQ("pod0_uid", state.PodIDByIPAtTime("1.2.3.4", 102));
  ASSERT_EQ("", state.PodIDByIPAtTime("1.2.3.4", 99));
  ASSERT_EQ("pod1_uid", state.PodIDByIPAtTime("1.2.3.5", 101));
  ASSERT_EQ("pod2_uid", state.PodIDByIPAtTime("1.2.3.5", 107));
  ASSERT_EQ("", state.PodIDByIPAtTime("1.2.3.4", 99));

  int64_t large_retention_time = 30;
  ASSERT_OK(state.CleanupExpiredMetadata(terminated_ip_pod_update.start_timestamp_ns(),
                                         large_retention_time));

  ASSERT_EQ("pod0_uid", state.PodIDByIPAtTime("1.2.3.4", 102));
  ASSERT_EQ("pod1_uid", state.PodIDByIPAtTime("1.2.3.5", 101));
  ASSERT_EQ("pod2_uid", state.PodIDByIPAtTime("1.2.3.5", 107));

  // Validate that the last running pod for an IP doesn't get cleaned up even
  // if it's past expiration.
  int64_t large_time_since_start = 100;
  int64_t small_retention_time = 3;
  ASSERT_OK(state.CleanupExpiredMetadata(
      running_ip_pod_update.start_timestamp_ns() + large_time_since_start, small_retention_time));

  ASSERT_EQ("pod0_uid", state.PodIDByIPAtTime("1.2.3.4", 102));  // still running
  ASSERT_EQ("", state.PodIDByIPAtTime("1.2.3.5", 101));
  ASSERT_EQ("pod2_uid", state.PodIDByIPAtTime("1.2.3.5", 107));  // still running

  pod_update.set_phase(px::shared::k8s::metadatapb::PodPhase::TERMINATED);
  EXPECT_OK(state.HandlePodUpdate(pod_update));

  // Trigger cleanup with the same params to check the terminated pod.
  ASSERT_OK(state.CleanupExpiredMetadata(
      running_ip_pod_update.start_timestamp_ns() + large_time_since_start, small_retention_time));
  ASSERT_EQ("", state.PodIDByIPAtTime("1.2.3.4", 102));          // now terminated
  ASSERT_EQ("pod2_uid", state.PodIDByIPAtTime("1.2.3.5", 107));  // still running
}

TEST(K8sMetadataStateTest, CleanupExpiredMetadata) {
  K8sMetadataState state;

  K8sMetadataState::NamespaceUpdate ns_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kRunningNamespaceUpdatePbTxt, &ns_update));

  K8sMetadataState::ContainerUpdate container_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kContainer0UpdatePbTxt, &container_update));

  K8sMetadataState::PodUpdate pod_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kPod0UpdatePbTxt, &pod_update));

  K8sMetadataState::PodUpdate terminated_ip_pod_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kPod1UpdatePbTxt, &terminated_ip_pod_update));

  K8sMetadataState::PodUpdate running_ip_pod_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kPod2UpdatePbTxt, &running_ip_pod_update));

  K8sMetadataState::ServiceUpdate service_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kRunningServiceUpdatePbTxt, &service_update));

  K8sMetadataState::ReplicaSetUpdate replica_set_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kReplicaSetUpdatePbTxt00, &replica_set_update));

  K8sMetadataState::DeploymentUpdate deployment_update;
  ASSERT_TRUE(TextFormat::MergeFromString(kDeploymentUpdatePbTxt03, &deployment_update));

  EXPECT_OK(state.HandleNamespaceUpdate(ns_update));
  EXPECT_OK(state.HandleContainerUpdate(container_update));
  EXPECT_OK(state.HandlePodUpdate(pod_update));
  EXPECT_OK(state.HandlePodUpdate(terminated_ip_pod_update));
  EXPECT_OK(state.HandlePodUpdate(running_ip_pod_update));
  EXPECT_OK(state.HandleServiceUpdate(service_update));
  EXPECT_OK(state.HandleReplicaSetUpdate(replica_set_update));
  EXPECT_OK(state.HandleDeploymentUpdate(deployment_update));

  int64_t current_time = 150ULL;

  // In this test, we give an expiry window that is large to make sure they live on.
  int64_t long_retention_time = current_time;
  ASSERT_OK(state.CleanupExpiredMetadata(current_time, long_retention_time));

  {
    const PodInfo* pod_info = state.PodInfoByID("pod0_uid");
    ASSERT_NE(pod_info, nullptr);

    const PodInfo* pod_info1 = state.PodInfoByID("pod1_uid");
    ASSERT_NE(pod_info1, nullptr);

    const PodInfo* pod_info2 = state.PodInfoByID("pod2_uid");
    ASSERT_NE(pod_info2, nullptr);

    ASSERT_EQ("pod0_uid", state.PodIDByIP("1.2.3.4"));
    ASSERT_EQ("pod2_uid", state.PodIDByIP("1.2.3.5"));

    const ServiceInfo* service_info = state.ServiceInfoByID("service0_uid");
    ASSERT_NE(service_info, nullptr);

    const NamespaceInfo* ns_info = state.NamespaceInfoByID("ns0_uid");
    ASSERT_NE(ns_info, nullptr);

    const ContainerInfo* container_info = state.ContainerInfoByID("container0_uid");
    ASSERT_NE(container_info, nullptr);

    const ReplicaSetInfo* replica_set_info = state.ReplicaSetInfoByID("rs0_uid");
    ASSERT_NE(replica_set_info, nullptr);

    const DeploymentInfo* deployment_info = state.DeploymentInfoByID("deployment_uid");
    ASSERT_NE(deployment_info, nullptr);
  }

  // Now we give them an expiry time that is short
  int64_t short_retention_time = 1ULL;
  ASSERT_OK(state.CleanupExpiredMetadata(current_time, short_retention_time));

  {
    const PodInfo* pod_info = state.PodInfoByID("pod0_uid");
    ASSERT_EQ(pod_info, nullptr);

    const PodInfo* pod_info1 = state.PodInfoByID("pod1_uid");
    ASSERT_EQ(pod_info1, nullptr);

    const PodInfo* pod_info2 = state.PodInfoByID("pod2_uid");
    ASSERT_NE(pod_info2, nullptr);

    ASSERT_EQ("", state.PodIDByIP("1.2.3.4"));
    ASSERT_EQ("pod2_uid", state.PodIDByIP("1.2.3.5"));

    const ServiceInfo* service_info = state.ServiceInfoByID("service0_uid");
    ASSERT_EQ(service_info, nullptr);

    const NamespaceInfo* ns_info = state.NamespaceInfoByID("ns0_uid");
    ASSERT_EQ(ns_info, nullptr);

    const ContainerInfo* container_info = state.ContainerInfoByID("container0_uid");
    ASSERT_EQ(container_info, nullptr);

    const ReplicaSetInfo* replica_set_info = state.ReplicaSetInfoByID("rs0_uid");
    ASSERT_EQ(replica_set_info, nullptr);

    const DeploymentInfo* deployment_info = state.DeploymentInfoByID("deployment_uid");
    ASSERT_EQ(deployment_info, nullptr);
  }
}

}  // namespace md
}  // namespace px
