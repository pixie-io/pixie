#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "src/common/event/api_impl.h"
#include "src/common/event/libuv.h"
#include "src/common/event/nats.h"
#include "src/common/system/config_mock.h"
#include "src/common/testing/event/simulated_time_system.h"
#include "src/common/testing/testing.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/vizier/messages/messagespb/messages.pb.h"
#include "src/vizier/services/agent/manager/k8s_update.h"
#include "src/vizier/services/agent/manager/manager.h"
#include "src/vizier/services/agent/manager/test_utils.h"

namespace pl {
namespace vizier {
namespace agent {

using ::pl::table_store::schema::Relation;
using ::pl::testing::proto::EqualsProto;
using ::pl::testing::proto::Partially;
using shared::metadatapb::MetadataType;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAreArray;

constexpr char kUpdate0[] = R"(
  namespace_update {
    name: "pl"
    uid: "namespace_1"
    start_timestamp_ns: 999
  }
)";

class K8sUpdateHandlerTest : public ::testing::Test {
 protected:
  void TearDown() override { dispatcher_->Exit(); }

  K8sUpdateHandlerTest() {
    start_monotonic_time_ = std::chrono::steady_clock::now();
    start_system_time_ = std::chrono::system_clock::now();
    time_system_ =
        std::make_unique<event::SimulatedTimeSystem>(start_monotonic_time_, start_system_time_);
    api_ = std::make_unique<pl::event::APIImpl>(time_system_.get());
    dispatcher_ = api_->AllocateDispatcher("manager");
    nats_conn_ = std::make_unique<FakeNATSConnector<pl::vizier::messages::VizierMessage>>();
    auto sys_config = system::MockConfig();
    EXPECT_CALL(sys_config, KernelTicksPerSecond()).WillRepeatedly(::testing::Return(10000000));
    EXPECT_CALL(sys_config, HasConfig()).WillRepeatedly(Return(true));
    EXPECT_CALL(sys_config, proc_path()).WillRepeatedly(ReturnRef(proc_path_));
    EXPECT_CALL(sys_config, sysfs_path()).WillRepeatedly(ReturnRef(sysfs_path_));

    fake_mds_manager_ = std::make_unique<FakeAgentMetadataStateManager>();
    k8s_update_handler_ = std::make_unique<K8sUpdateHandler>(
        dispatcher_.get(), fake_mds_manager_.get(), &agent_info_, nats_conn_.get());
  }

  event::MonotonicTimePoint start_monotonic_time_;
  event::SystemTimePoint start_system_time_;
  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  std::unique_ptr<event::APIImpl> api_;
  std::unique_ptr<event::Dispatcher> dispatcher_;
  std::unique_ptr<FakeAgentMetadataStateManager> fake_mds_manager_;
  std::unique_ptr<K8sUpdateHandler> k8s_update_handler_;
  std::unique_ptr<FakeNATSConnector<pl::vizier::messages::VizierMessage>> nats_conn_;
  agent::Info agent_info_;
  std::filesystem::path proc_path_;
  std::filesystem::path sysfs_path_;
};

TEST_F(K8sUpdateHandlerTest, HandleK8sUpdate) {
  auto sent_update = std::make_unique<messages::VizierMessage>();
  CHECK(google::protobuf::TextFormat::MergeFromString(
      kUpdate0, sent_update->mutable_k8s_metadata_message()->mutable_k8s_metadata_update()));
  auto s = k8s_update_handler_->HandleMessage(std::move(sent_update));
  EXPECT_OK(s);
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());
  auto recv_update = fake_mds_manager_->k8s_update(0);
  EXPECT_THAT(*recv_update, EqualsProto(kUpdate0));
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
