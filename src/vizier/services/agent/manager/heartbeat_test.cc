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
#include "src/vizier/services/agent/manager/heartbeat.h"
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

const char* kAgentUpdateInfoSchemaNoTablets = R"proto(
does_update_schema: true
schema {
  name: "relation0"
  columns {
    name: "time_"
    data_type: TIME64NS
  }
  columns {
    name: "count"
    data_type: INT64
  }
}
schema {
  name: "relation1"
  columns {
    name: "time_"
    data_type: TIME64NS
  }
  columns {
    name: "gauge"
    data_type: FLOAT64
  }
}
)proto";

class HeartbeatMessageHandlerTest : public ::testing::Test {
 protected:
  void TearDown() override { dispatcher_->Exit(); }

  HeartbeatMessageHandlerTest() {
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

    md_filter_ = md::AgentMetadataFilter::Create(
                     100, 0.01, md::AgentMetadataStateManager::MetadataFilterEntities())
                     .ConsumeValueOrDie();
    mds_manager_ = std::make_unique<md::AgentMetadataStateManager>(
        "host", 1, "pod", /* agent_id */ sole::uuid4(), true, sys_config, md_filter_.get());

    // Relation info with no tabletization.
    Relation relation0({types::TIME64NS, types::INT64}, {"time_", "count"});
    RelationInfo relation_info0("relation0", /* id */ 0, "desc0", relation0);
    // Relation info with no tabletization.
    Relation relation1({types::TIME64NS, types::FLOAT64}, {"time_", "gauge"});
    RelationInfo relation_info1("relation1", /* id */ 1, "desc1", relation1);
    std::vector<RelationInfo> relation_info_vec({relation_info0, relation_info1});
    // Pass relation info to the manager.
    relation_info_manager_ = std::make_unique<RelationInfoManager>();
    for (const auto& relation_info : relation_info_vec) {
      EXPECT_OK(relation_info_manager_->AddRelationInfo(relation_info));
    }

    agent_info_ = agent::Info{};
    agent_info_.capabilities.set_collects_data(true);

    heartbeat_handler_ = std::make_unique<HeartbeatMessageHandler>(
        dispatcher_.get(), mds_manager_.get(), relation_info_manager_.get(), &agent_info_,
        nats_conn_.get());
  }

  event::MonotonicTimePoint start_monotonic_time_;
  event::SystemTimePoint start_system_time_;
  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  std::unique_ptr<event::APIImpl> api_;
  std::unique_ptr<event::Dispatcher> dispatcher_;
  std::unique_ptr<md::AgentMetadataStateManager> mds_manager_;
  std::unique_ptr<RelationInfoManager> relation_info_manager_;
  std::unique_ptr<HeartbeatMessageHandler> heartbeat_handler_;
  std::unique_ptr<FakeNATSConnector<pl::vizier::messages::VizierMessage>> nats_conn_;
  agent::Info agent_info_;
  std::unique_ptr<md::AgentMetadataFilter> md_filter_;
  std::filesystem::path proc_path_;
  std::filesystem::path sysfs_path_;
};

TEST_F(HeartbeatMessageHandlerTest, InitialHeartbeatTimeout) {
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());

  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::seconds(6));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // If no ack received, hb manager should resend the same heartbeat.
  EXPECT_EQ(2, nats_conn_->published_msgs().size());
  hb = nats_conn_->published_msgs()[1].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());

  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(5 * 5000 + 1));
  ASSERT_DEATH(dispatcher_->Run(event::Dispatcher::RunType::NonBlock),
               "Timeout waiting for heartbeat ACK for seq_num=0");
}

TEST_F(HeartbeatMessageHandlerTest, HandleHeartbeat) {
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());
  EXPECT_THAT(hb.update_info(), Partially(EqualsProto(kAgentUpdateInfoSchemaNoTablets)));

  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(5 * 4000));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  auto hb_ack = std::make_unique<messages::VizierMessage>();
  auto hb_ack_msg = hb_ack->mutable_heartbeat_ack();
  hb_ack_msg->set_sequence_number(0);

  auto s = heartbeat_handler_->HandleMessage(std::move(hb_ack));

  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(5 * 5000 + 1));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(3, nats_conn_->published_msgs().size());
  hb = nats_conn_->published_msgs()[2].heartbeat();
  EXPECT_EQ(1, hb.sequence_number());
  // No schema should be included in subsequent heartbeats.
  EXPECT_EQ(0, hb.update_info().schema().size());
}

TEST_F(HeartbeatMessageHandlerTest, HandleHeartbeatMetadata) {
  ASSERT_OK(mds_manager_->PerformMetadataStateUpdate());

  ASSERT_OK(mds_manager_->metadata_filter()->InsertEntity(MetadataType::POD_NAME, "foo"));

  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  auto hb_ack = std::make_unique<messages::VizierMessage>();
  auto hb_ack_msg = hb_ack->mutable_heartbeat_ack();
  hb_ack_msg->set_sequence_number(0);

  auto s = heartbeat_handler_->HandleMessage(std::move(hb_ack));

  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());
  EXPECT_TRUE(hb.update_info().data().has_metadata_info());
  EXPECT_THAT(hb.update_info(), Partially(EqualsProto(kAgentUpdateInfoSchemaNoTablets)));

  auto metadata_info = hb.update_info().data().metadata_info();
  std::vector<MetadataType> types;
  for (auto i = 0; i < metadata_info.metadata_fields_size(); ++i) {
    types.push_back(metadata_info.metadata_fields(i));
  }
  EXPECT_THAT(types,
              UnorderedElementsAreArray(md::AgentMetadataStateManager::MetadataFilterEntities()));
  auto bf = md::AgentMetadataFilter::FromProto(metadata_info).ConsumeValueOrDie();
  EXPECT_TRUE(bf->ContainsEntity(MetadataType::POD_NAME, "foo"));
  EXPECT_FALSE(bf->ContainsEntity(MetadataType::SERVICE_NAME, "foo"));

  // Don't update the metadata filter when the k8s epoch hasn't changed.
  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(5 * 5000 + 1));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(2, nats_conn_->published_msgs().size());

  hb = nats_conn_->published_msgs()[1].heartbeat();
  EXPECT_EQ(1, hb.sequence_number());
  EXPECT_FALSE(hb.update_info().data().has_metadata_info());
}

TEST_F(HeartbeatMessageHandlerTest, HandleHeartbeatRelationUpdates) {
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());
  EXPECT_THAT(hb.update_info(), Partially(EqualsProto(kAgentUpdateInfoSchemaNoTablets)));

  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(5 * 4000));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  auto hb_ack = std::make_unique<messages::VizierMessage>();
  auto hb_ack_msg = hb_ack->mutable_heartbeat_ack();
  hb_ack_msg->set_sequence_number(0);

  auto s = heartbeat_handler_->HandleMessage(std::move(hb_ack));
  Relation relation2({types::TIME64NS, types::FLOAT64}, {"time_", "gauge"});
  RelationInfo relation_info2("relation2", /* id */ 1, "desc2", relation2);
  s = relation_info_manager_->AddRelationInfo(relation_info2);

  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(5 * 5000 + 1));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(3, nats_conn_->published_msgs().size());
  hb = nats_conn_->published_msgs()[2].heartbeat();
  EXPECT_EQ(1, hb.sequence_number());
  // Since relation info was updated it should publish the entire schema again.
  EXPECT_EQ(3, hb.update_info().schema().size());
}

class HeartbeatNackMessageHandlerTest : public ::testing::Test {
 protected:
  void TearDown() override { dispatcher_->Exit(); }

  HeartbeatNackMessageHandlerTest() {
    start_monotonic_time_ = std::chrono::steady_clock::now();
    start_system_time_ = std::chrono::system_clock::now();
    time_system_ =
        std::make_unique<event::SimulatedTimeSystem>(start_monotonic_time_, start_system_time_);
    api_ = std::make_unique<pl::event::APIImpl>(time_system_.get());
    dispatcher_ = api_->AllocateDispatcher("manager");
    nats_conn_ = std::make_unique<FakeNATSConnector<pl::vizier::messages::VizierMessage>>();

    agent_info_ = agent::Info{};
    agent_info_.capabilities.set_collects_data(true);

    heartbeat_nack_handler_ = std::make_unique<HeartbeatNackMessageHandler>(
        dispatcher_.get(), &agent_info_, nats_conn_.get(), [this]() -> Status {
          called_reregister_++;
          return Status::OK();
        });
  }

  event::MonotonicTimePoint start_monotonic_time_;
  event::SystemTimePoint start_system_time_;
  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  std::unique_ptr<event::APIImpl> api_;
  std::unique_ptr<event::Dispatcher> dispatcher_;
  std::unique_ptr<HeartbeatNackMessageHandler> heartbeat_nack_handler_;
  std::unique_ptr<FakeNATSConnector<pl::vizier::messages::VizierMessage>> nats_conn_;
  agent::Info agent_info_;
  std::unique_ptr<md::AgentMetadataFilter> md_filter_;
  int32_t called_reregister_ = 0;
};

TEST_F(HeartbeatNackMessageHandlerTest, ReceivedHeartbeatNack) {
  auto hb_nack = std::make_unique<messages::VizierMessage>();
  auto hb_nack_msg = hb_nack->mutable_heartbeat_nack();
  hb_nack_msg->set_reregister(false);

  ASSERT_DEATH([&]() { auto s = heartbeat_nack_handler_->HandleMessage(std::move(hb_nack)); }(),
               "Got a heartbeat NACK.");
}

TEST_F(HeartbeatNackMessageHandlerTest, ReceivedHeartbeatNackReregister) {
  auto hb_nack = std::make_unique<messages::VizierMessage>();
  auto hb_nack_msg = hb_nack->mutable_heartbeat_nack();
  hb_nack_msg->set_reregister(true);

  EXPECT_EQ(0, called_reregister_);
  auto s = heartbeat_nack_handler_->HandleMessage(std::move(hb_nack));
  ASSERT_OK(s);
  EXPECT_EQ(1, called_reregister_);
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
