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

#include <google/protobuf/text_format.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/common/event/event.h"
#include "src/common/testing/event/simulated_time_system.h"
#include "src/common/testing/testing.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/metadata/cgroup_metadata_reader_mock.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/metadata/state_manager.h"
#include "src/shared/metadata/test_utils.h"

DECLARE_string(proc_path);

namespace px {
namespace md {

using ResourceUpdate = px::shared::k8s::metadatapb::ResourceUpdate;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;

constexpr char kUpdate0_0Pbtxt[] = R"(
  namespace_update {
    name: "pl"
    uid: "namespace_1"
    start_timestamp_ns: 999
  }
)";

constexpr char kUpdate1_0Pbtxt[] = R"(
  container_update {
    name: "container_name1"
    cid: "container_id1"
    start_timestamp_ns: 1001
    container_state: CONTAINER_STATE_RUNNING
    message: "running message"
    reason: "running reason"
    container_type: CONTAINER_TYPE_DOCKER
  }
)";

constexpr char kUpdate1_1Pbtxt[] = R"(
  pod_update {
    name: "pod1"
    namespace: "pl"
    uid: "pod_id1"
    start_timestamp_ns: 1000
    container_ids: "container_id1"
    qos_class: QOS_CLASS_BURSTABLE
    phase: RUNNING
    message: "running message"
    reason: "running reason"
    owner_references: {
      kind: "ReplicaSet"
      name: "rs0"
      uid: "rs0_uid"
    }
  }
)";

// TODO(philkuz) how do we associate pods or upids with each service.
constexpr char kUpdate1_2Pbtxt[] = R"(
  service_update {
    name: "service1"
    namespace: "pl"
    uid: "service_id1"
    start_timestamp_ns: 1000
  }
)";

constexpr char kUpdate2_0Pbtxt[] = R"(
  container_update {
    name: "container_name2"
    cid: "container_id2"
    start_timestamp_ns: 1201
    container_state: CONTAINER_STATE_WAITING
    message: "waiting message"
    reason: "waiting reason"
    container_type: CONTAINER_TYPE_DOCKER
  }
)";

constexpr char kUpdate2_1Pbtxt[] = R"(
  pod_update {
    name: "pod2"
    namespace: "pl"
    uid: "pod_id2"
    start_timestamp_ns: 1200
    container_ids: "container_id2"
    qos_class: QOS_CLASS_BURSTABLE
    pod_ip: "1.1.1.1"
    phase: RUNNING
    message: "running message 2"
    reason: "running reason 2"
  }
)";

constexpr char kUpdate2_2Pbtxt[] = R"(
  pod_update {
    name: "pod3"
    namespace: "pl"
    uid: "pod_id3"
    start_timestamp_ns: 1200
    qos_class: QOS_CLASS_UNKNOWN
    phase: FAILED
    message: "failed message"
    reason: "failed reason"
  }
)";

constexpr char kUpdate3_0Pbtxt[] = R"(
  replica_set_update {
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
  }
)";

constexpr char kUpdate3_1Pbtxt[] = R"(
  deployment_update {
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
  }
)";

class FakePIDData : public MockCGroupMetadataReader {
 public:
  Status ReadPIDs(PodQOSClass qos, std::string_view pod_id, std::string_view container_id,
                  ContainerType type, absl::flat_hash_set<uint32_t>* pid_set) const override {
    if (qos == PodQOSClass::kBurstable && pod_id == "pod_id1" && container_id == "container_id1" &&
        type == ContainerType::kDocker) {
      *pid_set = {100, 200};
      return Status::OK();
    }

    return error::NotFound("no found");
  }

  StatusOr<uint64_t> CgroupID(PodQOSClass, std::string_view, std::string_view container_id,
                              ContainerType) const override {
    if (container_id == "container_id1") {
      return 77;
    }
    return error::NotFound("no found");
  }
};

// Generates some test updates for entry into the AgentMetadataState.
// This set include a pod, its container and a corresponding service.
void GenerateTestUpdateEvents(
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>* updates) {
  auto update0_0 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate0_0Pbtxt, update0_0.get()));
  updates->enqueue(std::move(update0_0));

  auto update1_0 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate1_0Pbtxt, update1_0.get()));
  updates->enqueue(std::move(update1_0));

  auto update1_1 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate1_1Pbtxt, update1_1.get()));
  updates->enqueue(std::move(update1_1));

  auto update1_2 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate1_2Pbtxt, update1_2.get()));
  updates->enqueue(std::move(update1_2));

  auto update3_0 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate3_0Pbtxt, update3_0.get()));
  updates->enqueue(std::move(update3_0));

  auto update3_1 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate3_1Pbtxt, update3_1.get()));
  updates->enqueue(std::move(update3_1));
}

// Generates some test updates for entry into the AgentMetadataState.
// This set include a pod and a container which don't belong to the node in question.
void GenerateTestUpdateEventsForNonExistentPod(
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>* updates) {
  auto update2_0 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate2_0Pbtxt, update2_0.get()));
  updates->enqueue(std::move(update2_0));

  auto update2_1 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate2_1Pbtxt, update2_1.get()));
  updates->enqueue(std::move(update2_1));

  auto update2_2 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate2_2Pbtxt, update2_2.get()));
  updates->enqueue(std::move(update2_2));
}

class AgentMetadataStateTest : public ::testing::Test {
 protected:
  static constexpr int kASID = 123;
  static constexpr int kPID = 654;
  static constexpr char kHostname[] = "myhost";
  static constexpr char kPodName[] = "mypod";
  static constexpr char kVizierName[] = "myvizier";
  static constexpr char kVizierNamespace[] = "myviziernamespace";

  AgentMetadataStateTest()
      : agent_id_(sole::uuid4()),
        vizier_id_(sole::uuid4()),
        start_monotonic_time_(std::chrono::steady_clock::now()),
        start_system_time_(std::chrono::system_clock::now()),
        time_system_(std::make_unique<event::SimulatedTimeSystem>(start_monotonic_time_,
                                                                  start_system_time_)),
        metadata_state_(kHostname, kASID, kPID, agent_id_, kPodName, vizier_id_, kVizierName,
                        kVizierNamespace, time_system_.get()) {}

  sole::uuid agent_id_;
  sole::uuid vizier_id_;
  event::MonotonicTimePoint start_monotonic_time_;
  event::SystemTimePoint start_system_time_;
  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  AgentMetadataState metadata_state_;
  TestAgentMetadataFilter md_filter_;
};

TEST_F(AgentMetadataStateTest, initialize_md_state) {
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> updates;
  GenerateTestUpdateEvents(&updates);

  EXPECT_OK(ApplyK8sUpdates(2000 /*ts*/, &metadata_state_, &md_filter_, &updates));
  EXPECT_EQ(0, updates.size_approx());

  EXPECT_EQ("myhost", metadata_state_.hostname());
  EXPECT_EQ("mypod", metadata_state_.pod_name());
  EXPECT_EQ(123, metadata_state_.asid());
  EXPECT_EQ(654, metadata_state_.pid());
  EXPECT_EQ(agent_id_.str(), metadata_state_.agent_id().str());
  EXPECT_EQ(vizier_id_.str(), metadata_state_.vizier_id().str());
  EXPECT_EQ("myvizier", metadata_state_.vizier_name());
  EXPECT_EQ("myviziernamespace", metadata_state_.vizier_namespace());

  K8sMetadataState* state = metadata_state_.k8s_metadata_state();
  EXPECT_THAT(state->pods_by_name(), UnorderedElementsAre(Pair(Pair("pl", "pod1"), "pod_id1")));
  EXPECT_EQ("pod_id1", state->PodIDByName({"pl", "pod1"}));

  auto* pod_info = state->PodInfoByID("pod_id1");
  ASSERT_NE(nullptr, pod_info);
  EXPECT_EQ(1000, pod_info->start_time_ns());
  EXPECT_EQ("pod_id1", pod_info->uid());
  EXPECT_EQ("pod1", pod_info->name());
  EXPECT_EQ("pl", pod_info->ns());
  EXPECT_EQ(PodQOSClass::kBurstable, pod_info->qos_class());
  EXPECT_THAT(pod_info->containers(), UnorderedElementsAre("container_id1"));
  EXPECT_EQ(PodPhase::kRunning, pod_info->phase());
  EXPECT_EQ("running message", pod_info->phase_message());
  EXPECT_EQ("running reason", pod_info->phase_reason());
  EXPECT_THAT(pod_info->owner_references(),
              UnorderedElementsAre(OwnerReference{"rs0_uid", "rs0", "ReplicaSet"}));
  // test that we can access replica set which is the owner of this pod
  EXPECT_NE(nullptr, state->ReplicaSetInfoByID(pod_info->owner_references().begin()->uid));

  auto* container_info = state->ContainerInfoByID("container_id1");
  ASSERT_NE(nullptr, container_info);
  EXPECT_EQ("container_id1", container_info->cid());
  EXPECT_EQ("pod_id1", container_info->pod_id());
  EXPECT_EQ(ContainerState::kRunning, container_info->state());
  EXPECT_EQ("running message", container_info->state_message());
  EXPECT_EQ("running reason", container_info->state_reason());

  auto* service_info = state->ServiceInfoByID("service_id1");
  ASSERT_NE(nullptr, service_info);
  EXPECT_EQ(1000, service_info->start_time_ns());
  EXPECT_EQ("service_id1", service_info->uid());
  EXPECT_EQ("service1", service_info->name());
  EXPECT_EQ("pl", service_info->ns());

  auto* ns_info = state->NamespaceInfoByID("namespace_1");
  ASSERT_NE(nullptr, ns_info);
  EXPECT_EQ(999, ns_info->start_time_ns());
  EXPECT_EQ("namespace_1", ns_info->uid());
  EXPECT_EQ("pl", ns_info->name());
  EXPECT_EQ("pl", ns_info->ns());

  auto* rs_info = state->ReplicaSetInfoByID("rs0_uid");
  ASSERT_NE(nullptr, rs_info);
  EXPECT_EQ(101, rs_info->start_time_ns());
  EXPECT_EQ(0, rs_info->stop_time_ns());
  EXPECT_EQ("rs0_uid", rs_info->uid());
  EXPECT_EQ("rs0", rs_info->name());
  EXPECT_EQ("ns0", rs_info->ns());
  EXPECT_EQ(5, rs_info->replicas());
  EXPECT_EQ(5, rs_info->fully_labeled_replicas());
  EXPECT_EQ(3, rs_info->ready_replicas());
  EXPECT_EQ(3, rs_info->available_replicas());
  EXPECT_EQ(5, rs_info->observed_generation());
  EXPECT_EQ(5, rs_info->requested_replicas());

  EXPECT_THAT(rs_info->owner_references(),
              UnorderedElementsAre(OwnerReference{"deployment_uid", "deployment1", "Deployment"}));

  auto* dep_info = state->DeploymentInfoByID("deployment_uid");
  ASSERT_NE(nullptr, dep_info);
  EXPECT_EQ("deployment_uid", dep_info->uid());
  EXPECT_EQ("deployment1", dep_info->name());
  EXPECT_EQ("ns0", dep_info->ns());
  EXPECT_EQ(5, dep_info->replicas());
  EXPECT_EQ(5, dep_info->updated_replicas());
  EXPECT_EQ(3, dep_info->ready_replicas());
  EXPECT_EQ(3, dep_info->available_replicas());
  EXPECT_EQ(2, dep_info->unavailable_replicas());
  EXPECT_EQ(5, dep_info->observed_generation());
  EXPECT_EQ(5, dep_info->requested_replicas());
  EXPECT_EQ(101, dep_info->start_time_ns());
  EXPECT_EQ(0, dep_info->stop_time_ns());
  EXPECT_EQ(2, dep_info->conditions().size());
  EXPECT_EQ(ConditionStatus::kTrue, dep_info->conditions()[DeploymentConditionType::kAvailable]);
  EXPECT_EQ(ConditionStatus::kTrue, dep_info->conditions()[DeploymentConditionType::kProgressing]);
}

TEST_F(AgentMetadataStateTest, pid_created) {
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> updates;
  GenerateTestUpdateEvents(&updates);

  EXPECT_OK(ApplyK8sUpdates(2000 /*ts*/, &metadata_state_, &md_filter_, &updates));

  moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>> events;
  FakePIDData md_reader;
  LOG(INFO) << metadata_state_.DebugString();

  const auto proc_path = testing::BazelRunfilePath("src/shared/metadata/testdata/proc");
  PX_SET_FOR_SCOPE(FLAGS_proc_path, proc_path.string());
  system::ProcParser proc_parser;
  EXPECT_OK(ProcessPIDUpdates(1000, proc_parser, &metadata_state_, &md_reader, &events));

  std::unique_ptr<PIDStatusEvent> event;
  std::vector<PIDStartedEvent> pids_started;

  while (events.try_dequeue(event)) {
    if (event->type == PIDStatusEventType::kStarted) {
      pids_started.emplace_back(*static_cast<PIDStartedEvent*>(event.get()));
    } else {
      FAIL() << "Only expected started events";
    }
  }

  PIDInfo pid1(UPID(kASID, 100 /*pid*/, 1000 /*ts*/), "", "cmdline100", "container_id1");
  PIDInfo pid2(UPID(kASID /*asid*/, 200 /*pid*/, 2000 /*ts*/), "", "cmdline200", "container_id1");
  UPID parent(kASID, 3260 /*pid*/, 500 /*ts*/);
  pid1.set_parent_upid(parent);
  pid2.set_parent_upid(parent);

  EXPECT_EQ(2, pids_started.size());
  EXPECT_THAT(pids_started, UnorderedElementsAre(PIDStartedEvent{pid1}, PIDStartedEvent{pid2}));
  EXPECT_EQ(metadata_state_.ParentUPID(pid1.upid()), parent);
}

TEST_F(AgentMetadataStateTest, short_lived_process_resolves_to_ancestor) {
  UPID parent(kASID, 100 /*pid*/, 1000 /*ts*/);
  UPID child(kASID, 300 /*pid*/, 3000 /*ts*/);
  UPID grandchild(kASID, 301 /*pid*/, 3001 /*ts*/);
  metadata_state_.AddUPID(parent,
                          std::make_unique<PIDInfo>(parent, "", "cmdline100", "container_id1"));
  metadata_state_.AddProcessParent(child, parent);
  metadata_state_.AddProcessParent(grandchild, child);

  EXPECT_EQ(metadata_state_.ParentUPID(grandchild), child);
  EXPECT_EQ(metadata_state_.ParentUPID(child), parent);
  EXPECT_EQ(metadata_state_.GetPIDByUPID(grandchild), nullptr);
  EXPECT_EQ(metadata_state_.GetPIDByUPIDOrAncestor(UPID(kASID, 999 /*pid*/, 9 /*ts*/)), nullptr);
  ASSERT_NE(metadata_state_.GetPIDByUPIDOrAncestor(grandchild), nullptr);
  EXPECT_EQ(metadata_state_.GetPIDByUPIDOrAncestor(grandchild)->cid(), "container_id1");

  auto cloned = metadata_state_.CloneToShared();
  ASSERT_NE(cloned->GetPIDByUPIDOrAncestor(grandchild), nullptr);
  EXPECT_EQ(cloned->GetPIDByUPIDOrAncestor(grandchild)->upid(), parent);
}

TEST_F(AgentMetadataStateTest, process_resolves_to_the_container_of_its_cgroup) {
  UPID process(kASID, 300 /*pid*/, 3000 /*ts*/);
  UPID child(kASID, 301 /*pid*/, 3001 /*ts*/);
  UPID other(kASID, 302 /*pid*/, 3002 /*ts*/);
  metadata_state_.SetContainerCgroupID("container_id1", 77);
  metadata_state_.AddProcessCgroup(process, 77);
  metadata_state_.AddProcessCgroup(other, 78);
  metadata_state_.AddProcessParent(child, process);

  EXPECT_EQ(metadata_state_.ContainerIDByUPID(process), "container_id1");
  EXPECT_EQ(metadata_state_.ContainerIDByUPID(child), "container_id1");
  EXPECT_EQ(metadata_state_.ContainerIDByUPID(other), "");
  EXPECT_EQ(metadata_state_.CloneToShared()->ContainerIDByUPID(process), "container_id1");
}

TEST_F(AgentMetadataStateTest, process_cgroups_are_bounded) {
  metadata_state_.SetContainerCgroupID("container_id1", 77);
  for (uint32_t i = 0; i <= AgentMetadataState::kMaxProcessParents; ++i) {
    metadata_state_.AddProcessCgroup(UPID(kASID, 1000 + i, 1 /*ts*/), 77);
  }

  EXPECT_EQ(metadata_state_.ContainerIDByUPID(UPID(kASID, 1000 /*pid*/, 1 /*ts*/)), "");
  EXPECT_EQ(metadata_state_.ContainerIDByUPID(UPID(kASID, 1001 /*pid*/, 1 /*ts*/)),
            "container_id1");
}

TEST_F(AgentMetadataStateTest, process_cgroups_are_applied_after_the_scan) {
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> updates;
  GenerateTestUpdateEvents(&updates);
  ASSERT_OK(ApplyK8sUpdates(2000 /*ts*/, &metadata_state_, &md_filter_, &updates));
  FakePIDData md_reader;
  system::ProcParser proc_parser;
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>> pid_updates;
  ASSERT_OK(ProcessPIDUpdates(1000, proc_parser, &metadata_state_, &md_reader, &pid_updates));

  UPID process(kASID, 300 /*pid*/, 3000 /*ts*/);
  moodycamel::BlockingConcurrentQueue<std::pair<UPID, uint64_t>> process_cgroups;
  process_cgroups.enqueue({process, 77});
  ApplyProcessCgroups(&metadata_state_, &process_cgroups);

  EXPECT_EQ(0, process_cgroups.size_approx());
  EXPECT_EQ(metadata_state_.ContainerIDByUPID(process), "container_id1");
}

TEST_F(AgentMetadataStateTest, reported_parent_fills_missing_parent) {
  UPID parent(kASID, 100 /*pid*/, 1000 /*ts*/);
  UPID child(kASID, 300 /*pid*/, 3000 /*ts*/);
  metadata_state_.AddUPID(child,
                          std::make_unique<PIDInfo>(child, "", "cmdline300", "container_id1"));
  metadata_state_.AddProcessParent(child, parent);

  EXPECT_EQ(metadata_state_.ParentUPID(child), parent);
}

TEST_F(AgentMetadataStateTest, process_parents_are_listed) {
  UPID init(kASID, 1 /*pid*/, 10 /*ts*/);
  UPID parent(kASID, 100 /*pid*/, 1000 /*ts*/);
  UPID child(kASID, 300 /*pid*/, 3000 /*ts*/);
  auto parent_info = std::make_unique<PIDInfo>(parent, "", "cmdline100", "container_id1");
  parent_info->set_parent_upid(init);
  metadata_state_.AddUPID(parent, std::move(parent_info));
  metadata_state_.AddUPID(init, std::make_unique<PIDInfo>(init, "", "cmdline1", "container_id1"));
  metadata_state_.AddProcessParent(parent, init);
  metadata_state_.AddProcessParent(child, parent);

  EXPECT_THAT(metadata_state_.ProcessParents(),
              UnorderedElementsAre(Pair(child, parent), Pair(parent, init)));
}

TEST_F(AgentMetadataStateTest, process_parents_are_bounded) {
  UPID parent(kASID, 100 /*pid*/, 1000 /*ts*/);
  for (uint32_t i = 0; i <= AgentMetadataState::kMaxProcessParents; ++i) {
    metadata_state_.AddProcessParent(UPID(kASID, 1000 + i, 1 /*ts*/), parent);
  }

  EXPECT_EQ(metadata_state_.ParentUPID(UPID(kASID, 1000 /*pid*/, 1 /*ts*/)), UPID());
  EXPECT_EQ(metadata_state_.ParentUPID(UPID(kASID, 1001 /*pid*/, 1 /*ts*/)), parent);
}

TEST_F(AgentMetadataStateTest, process_parents_are_applied) {
  UPID parent(kASID, 100 /*pid*/, 1000 /*ts*/);
  UPID child(kASID, 300 /*pid*/, 3000 /*ts*/);
  moodycamel::BlockingConcurrentQueue<std::pair<UPID, UPID>> process_parents;
  process_parents.enqueue({child, parent});

  ApplyProcessParents(&metadata_state_, &process_parents);

  EXPECT_EQ(0, process_parents.size_approx());
  EXPECT_EQ(metadata_state_.ParentUPID(child), parent);
}

TEST_F(AgentMetadataStateTest, insert_into_filter) {
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> updates;
  GenerateTestUpdateEvents(&updates);

  EXPECT_OK(ApplyK8sUpdates(2000 /*ts*/, &metadata_state_, &md_filter_, &updates));
  EXPECT_EQ(0, updates.size_approx());

  EXPECT_THAT(md_filter_.metadata_types(),
              UnorderedElementsAre(MetadataType::SERVICE_ID, MetadataType::SERVICE_NAME,
                                   MetadataType::POD_ID, MetadataType::POD_NAME,
                                   MetadataType::CONTAINER_ID));
  EXPECT_THAT(md_filter_.inserted_entities(),
              ElementsAre("CONTAINER_ID=container_id1", "POD_ID=pod_id1", "POD_NAME=pod1",
                          "POD_NAME=pl/pod1", "SERVICE_ID=service_id1", "SERVICE_NAME=service1",
                          "SERVICE_NAME=pl/service1"));
}

TEST_F(AgentMetadataStateTest, cidr_test) {
  AgentMetadataStateManagerImpl mgr(
      "test_host", /*asid*/ 0, /*pid*/ 987, "test_pod",
      /*id*/ sole::uuid4(),
      /*collects_data*/ true, px::system::Config::GetInstance(), &md_filter_,
      /*vizier_id*/ sole::uuid4(), "test_vizier", "test_vizier_namespace", time_system_.get());

  EXPECT_OK(mgr.PerformMetadataStateUpdate());
  // Should not be updated yet.
  EXPECT_EQ(0, mgr.CurrentAgentMetadataState()->k8s_metadata_state().pod_cidrs().size());
  EXPECT_FALSE(mgr.CurrentAgentMetadataState()->k8s_metadata_state().service_cidr().has_value());

  CIDRBlock pod_cidr0;
  CIDRBlock pod_cidr1;
  CIDRBlock svc_cidr;

  ASSERT_OK(ParseCIDRBlock("1.2.3.4/10", &pod_cidr0));
  ASSERT_OK(ParseCIDRBlock("16.17.18.19/10", &pod_cidr1));
  ASSERT_OK(ParseCIDRBlock("1.2.3.7/10", &svc_cidr));

  mgr.SetPodCIDR(std::vector<CIDRBlock>{pod_cidr0, pod_cidr1});
  mgr.SetServiceCIDR(svc_cidr);

  // Should not be updated yet.
  EXPECT_EQ(0, mgr.CurrentAgentMetadataState()->k8s_metadata_state().pod_cidrs().size());
  EXPECT_FALSE(mgr.CurrentAgentMetadataState()->k8s_metadata_state().service_cidr().has_value());

  // Cause an update.
  EXPECT_OK(mgr.PerformMetadataStateUpdate());
  auto md_state = mgr.CurrentAgentMetadataState();
  const auto& k8s_state = md_state->k8s_metadata_state();

  auto md_pod_cidrs = k8s_state.pod_cidrs();
  EXPECT_THAT(md_pod_cidrs, ElementsAre(pod_cidr0, pod_cidr1));
  auto md_svc_cidr = k8s_state.service_cidr();
  EXPECT_TRUE(md_svc_cidr.has_value());
  EXPECT_EQ(svc_cidr, md_svc_cidr.value());
}

}  // namespace md
}  // namespace px
