#include <google/protobuf/text_format.h>
#include <string>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/metadata/cgroup_metadata_reader_mock.h"
#include "src/shared/metadata/state_manager.h"

namespace pl {
namespace md {

using pl::shared::k8s::metadatapb::MetadataResourceType;
using ResourceUpdate = pl::shared::k8s::metadatapb::ResourceUpdate;

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

constexpr char kUpdate1_0Pbtxt[] = R"(
  container_update {
    name: "container_name1"
    cid: "container_id1"
    start_timestamp_ns: 1001
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
  }
)";

class FakePIDData : public MockCGroupMetadataReader {
 public:
  Status ReadPIDs(PodQOSClass qos, std::string_view pod_id, std::string_view container_id,
                  absl::flat_hash_set<uint32_t>* pid_set) const override {
    if (qos == PodQOSClass::kBurstable && pod_id == "pod_id1" && container_id == "container_id1") {
      *pid_set = {100, 200};
      return Status::OK();
    }

    return error::NotFound("no found");
  }

  int64_t ReadPIDStartTimeTicks(uint32_t pid) const override {
    if (pid == 100) {
      return 1000;
    }

    if (pid == 200) {
      return 2000;
    }

    return 0;
  }

  std::string ReadPIDCmdline(uint32_t pid) const override {
    if (pid == 100) {
      return "cmdline100";
    }

    if (pid == 200) {
      return "cmdline200";
    }

    return "";
  }

  bool PodDirExists(const PodInfo& pod_info) const override {
    if (pod_info.uid() == "pod_id1") {
      return true;
    }

    return false;
  }
};

// Generates some test updates for entry into the AgentMetadataState.
// This set include a pod, its container and a corresponding service.
void GenerateTestUpdateEvents(
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>* updates) {
  auto update1_0 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate1_0Pbtxt, update1_0.get()));
  updates->enqueue(std::move(update1_0));

  auto update1_1 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate1_1Pbtxt, update1_1.get()));
  updates->enqueue(std::move(update1_1));

  auto update1_2 = std::make_unique<ResourceUpdate>();
  CHECK(google::protobuf::TextFormat::MergeFromString(kUpdate1_2Pbtxt, update1_2.get()));
  updates->enqueue(std::move(update1_2));
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
}

class AgentMetadataStateTest : public ::testing::Test {
 protected:
  static constexpr int kASID = 123;

  AgentMetadataStateTest() : metadata_state_(kASID) {}

  AgentMetadataState metadata_state_;
};

TEST_F(AgentMetadataStateTest, initialize_md_state) {
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> updates;
  GenerateTestUpdateEvents(&updates);

  EXPECT_OK(AgentMetadataStateManager::ApplyK8sUpdates(2000 /*ts*/, &metadata_state_, &updates));
  EXPECT_EQ(0, updates.size_approx());

  EXPECT_EQ(123, metadata_state_.asid());

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

  auto* container_info = state->ContainerInfoByID("container_id1");
  ASSERT_NE(nullptr, container_info);
  EXPECT_EQ("container_id1", container_info->cid());
  EXPECT_EQ("pod_id1", container_info->pod_id());

  EXPECT_THAT(state->services_by_name(),
              UnorderedElementsAre(Pair(Pair("pl", "service1"), "service_id1")));
  EXPECT_EQ("service_id1", state->ServiceIDByName({"pl", "service1"}));

  auto* service_info = state->ServiceInfoByID("service_id1");
  ASSERT_NE(nullptr, service_info);
  EXPECT_EQ(1000, service_info->start_time_ns());
  EXPECT_EQ("service_id1", service_info->uid());
  EXPECT_EQ("service1", service_info->name());
  EXPECT_EQ("pl", service_info->ns());
}

TEST_F(AgentMetadataStateTest, remove_dead_pods) {
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> updates;
  GenerateTestUpdateEvents(&updates);
  GenerateTestUpdateEventsForNonExistentPod(&updates);

  ASSERT_OK(AgentMetadataStateManager::ApplyK8sUpdates(/*ts*/ 2000, &metadata_state_, &updates));
  ASSERT_EQ(0, updates.size_approx());

  FakePIDData md_reader;
  K8sMetadataState* state = metadata_state_.k8s_metadata_state();

  const PodInfo* pod_info;

  // Check state before call to RemoveDeadPods().
  EXPECT_EQ(state->pods_by_name().size(), 2);

  pod_info = state->PodInfoByID("pod_id1");
  ASSERT_NE(nullptr, pod_info);
  EXPECT_EQ(0, pod_info->stop_time_ns());

  pod_info = state->PodInfoByID("pod_id2");
  ASSERT_NE(nullptr, pod_info);
  EXPECT_EQ(0, pod_info->stop_time_ns());

  AgentMetadataStateManager::RemoveDeadPods(/*ts*/ 100, &metadata_state_, &md_reader);

  // Expected state after call to RemoveDeadPods().
  EXPECT_EQ(state->pods_by_name().size(), 2);

  // This pod should still be alive, as indicated by stop_time_ns == 0.
  pod_info = state->PodInfoByID("pod_id1");
  ASSERT_NE(nullptr, pod_info);
  EXPECT_EQ(0, pod_info->stop_time_ns());

  // This pod should still be marked as dead, as indicated by stop_time_ns != 0.
  pod_info = state->PodInfoByID("pod_id2");
  ASSERT_NE(nullptr, pod_info);
  EXPECT_NE(0, pod_info->stop_time_ns());
}

TEST_F(AgentMetadataStateTest, pid_created) {
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> updates;
  GenerateTestUpdateEvents(&updates);

  EXPECT_OK(AgentMetadataStateManager::ApplyK8sUpdates(2000 /*ts*/, &metadata_state_, &updates));

  moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>> events;
  FakePIDData md_reader;
  LOG(INFO) << metadata_state_.DebugString();
  EXPECT_OK(
      AgentMetadataStateManager::ProcessPIDUpdates(1000, &metadata_state_, &md_reader, &events));

  std::unique_ptr<PIDStatusEvent> event;
  std::vector<PIDStartedEvent> pids_started;

  while (events.try_dequeue(event)) {
    if (event->type == PIDStatusEventType::kStarted) {
      pids_started.emplace_back(*static_cast<PIDStartedEvent*>(event.get()));
    } else {
      FAIL() << "Only expected started events";
    }
  }

  PIDInfo pid1(UPID(kASID, 100 /*pid*/, 1000 /*ts*/), "cmdline100", "container_id1");
  PIDInfo pid2(UPID(kASID /*asid*/, 200 /*pid*/, 2000 /*ts*/), "cmdline200", "container_id1");

  EXPECT_EQ(2, pids_started.size());
  EXPECT_THAT(pids_started, UnorderedElementsAre(PIDStartedEvent{pid1}, PIDStartedEvent{pid2}));
}

}  // namespace md
}  // namespace pl
