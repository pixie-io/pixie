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

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "src/common/event/api_impl.h"
#include "src/common/event/libuv.h"
#include "src/common/event/nats.h"
#include "src/common/testing/event/simulated_time_system.h"
#include "src/common/testing/testing.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/vizier/messages/messagespb/messages.pb.h"
#include "src/vizier/services/agent/shared/manager/heartbeat.h"
#include "src/vizier/services/agent/shared/manager/manager.h"
#include "src/vizier/services/agent/shared/manager/test_utils.h"

namespace px {
namespace vizier {
namespace agent {

using ::px::table_store::schema::Relation;
using ::px::testing::proto::EqualsProto;
using ::px::testing::proto::Partially;
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

const char* kAgentPIDStartedTemplate = R"proto(
process_created {
  upid {
    low: $0
    high: 4294967298
  }
  start_timestamp_ns: $0
  cmdline: "./a_command"
  cid: "example_container"
}
)proto";

const char* kAgentPIDTerminatedTemplate = R"proto(
process_terminated {
  upid {
    low: $0
    high: 4294967298
  }
  stop_timestamp_ns: $1
}
)proto";

int64_t time_to_nanos(event::MonotonicTimePoint time) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}

class HeartbeatMessageHandlerTest : public ::testing::Test {
 protected:
  void TearDown() override { dispatcher_->Exit(); }

  HeartbeatMessageHandlerTest() {
    time_system_ = std::make_unique<event::SimulatedTimeSystem>();
    api_ = std::make_unique<px::event::APIImpl>(time_system_.get());
    dispatcher_ = api_->AllocateDispatcher("manager");
    nats_conn_ = std::make_unique<FakeNATSConnector<px::vizier::messages::VizierMessage>>();

    md_filter_ = md::AgentMetadataFilter::Create(10, 0.1, {md::MetadataType::SERVICE_NAME})
                     .ConsumeValueOrDie();
    EXPECT_OK(md_filter_->InsertEntity(md::MetadataType::SERVICE_NAME, "pl/service"));
    mds_manager_ = std::make_unique<FakeAgentMetadataStateManager>(md_filter_.get());

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

  void CheckFilterElements(const messages::AgentDataInfo& data_info,
                           absl::flat_hash_set<std::string> present,
                           absl::flat_hash_set<std::string> not_present) {
    auto filter = md::AgentMetadataFilter::FromProto(data_info.metadata_info()).ConsumeValueOrDie();
    for (const auto& str : present) {
      EXPECT_TRUE(filter->ContainsEntity(md::MetadataType::SERVICE_NAME, str));
    }
    for (const auto& str : not_present) {
      EXPECT_FALSE(filter->ContainsEntity(md::MetadataType::SERVICE_NAME, str));
    }
  }

  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  std::unique_ptr<event::APIImpl> api_;
  std::unique_ptr<event::Dispatcher> dispatcher_;
  std::unique_ptr<FakeAgentMetadataStateManager> mds_manager_;
  std::unique_ptr<RelationInfoManager> relation_info_manager_;
  std::unique_ptr<HeartbeatMessageHandler> heartbeat_handler_;
  std::unique_ptr<FakeNATSConnector<px::vizier::messages::VizierMessage>> nats_conn_;
  agent::Info agent_info_;
  std::unique_ptr<md::AgentMetadataFilter> md_filter_;
  std::filesystem::path proc_path_;
  std::filesystem::path sysfs_path_;
};

TEST_F(HeartbeatMessageHandlerTest, HandleHeartbeat) {
  auto start_time_nanos = time_to_nanos(time_system_->MonotonicTime());
  md::UPID upid(1, 2, start_time_nanos);
  md::PIDInfo pid_info(upid, "", "./a_command", "example_container");
  mds_manager_->AddPIDStatusEvent(std::make_unique<md::PIDStartedEvent>(pid_info));

  // Publish pid updates.
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());
  // Check that all of the information we expect is there.
  EXPECT_THAT(hb.update_info(), Partially(EqualsProto(kAgentUpdateInfoSchemaNoTablets)));
  EXPECT_THAT(hb.update_info(),
              Partially(EqualsProto(absl::Substitute(kAgentPIDStartedTemplate, start_time_nanos))));
  CheckFilterElements(hb.update_info().data(), {"pl/service"}, {"pl/another_service"});

  time_system_->Sleep(std::chrono::milliseconds(5 * 4000));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  auto hb_ack = std::make_unique<messages::VizierMessage>();
  auto hb_ack_msg = hb_ack->mutable_heartbeat_ack();
  hb_ack_msg->set_sequence_number(0);

  auto s = heartbeat_handler_->HandleMessage(std::move(hb_ack));

  auto sleep = std::chrono::milliseconds(5 * 5000 + 1);
  auto stop_time_nanos = time_to_nanos(time_system_->MonotonicTime() + sleep);

  mds_manager_->AddPIDStatusEvent(
      std::make_unique<md::PIDTerminatedEvent>(pid_info.upid(), stop_time_nanos));

  time_system_->Sleep(sleep);
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(3, nats_conn_->published_msgs().size());
  hb = nats_conn_->published_msgs()[2].heartbeat();
  EXPECT_EQ(1, hb.sequence_number());
  // No schema should be included in subsequent heartbeat.
  EXPECT_EQ(0, hb.update_info().schema().size());
  // New PID updates should be included in subsequent heartbeat.
  EXPECT_THAT(hb.update_info(),
              Partially(EqualsProto(absl::Substitute(kAgentPIDTerminatedTemplate, start_time_nanos,
                                                     stop_time_nanos))));
  // No new metadata filter should be included in subsequent heartbeat.
  EXPECT_FALSE(hb.update_info().data().has_metadata_info());
}

TEST_F(HeartbeatMessageHandlerTest, HandleHeartbeatLongPIDCmdLine) {
  auto start_time_nanos = time_to_nanos(time_system_->MonotonicTime());
  md::UPID upid(1, 2, start_time_nanos);
  std::string s(4097, 'a');
  md::PIDInfo pid_info(upid, "", s, "example_container");
  mds_manager_->AddPIDStatusEvent(std::make_unique<md::PIDStartedEvent>(pid_info));

  // Publish pid updates.
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());
  EXPECT_THAT(hb.update_info().process_created(0).cmdline(), ::testing::EndsWith("[TRUNCATED]"));
}

TEST_F(HeartbeatMessageHandlerTest, HandleHeartbeatMetadataChange) {
  // Tthe metadata info should be resent when it changes.
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());
  // Check that all of the information we expect is there.
  CheckFilterElements(hb.update_info().data(), {"pl/service"}, {"pl/another_service"});

  time_system_->Sleep(std::chrono::milliseconds(5 * 4000));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  auto hb_ack = std::make_unique<messages::VizierMessage>();
  auto hb_ack_msg = hb_ack->mutable_heartbeat_ack();
  hb_ack_msg->set_sequence_number(0);

  auto s = heartbeat_handler_->HandleMessage(std::move(hb_ack));

  EXPECT_OK(md_filter_->InsertEntity(md::MetadataType::SERVICE_NAME, "pl/another_service"));

  time_system_->Sleep(std::chrono::milliseconds(5 * 5000 + 1));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(3, nats_conn_->published_msgs().size());
  hb = nats_conn_->published_msgs()[2].heartbeat();
  EXPECT_EQ(1, hb.sequence_number());
  CheckFilterElements(hb.update_info().data(), {"pl/service", "pl/another_service"},
                      {"pl/another_service_2"});
}

TEST_F(HeartbeatMessageHandlerTest, HandleHeartbeatMetadataAfterDisable) {
  // Even if the metadata info didn't change, if the heartbeat was disabled then re-enabled,
  // the metadata info should be resent.
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());
  // Check that all of the information we expect is there.
  CheckFilterElements(hb.update_info().data(), {"pl/service"}, {"pl/another_service"});

  time_system_->Sleep(std::chrono::milliseconds(5 * 4000));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  auto hb_ack = std::make_unique<messages::VizierMessage>();
  auto hb_ack_msg = hb_ack->mutable_heartbeat_ack();
  hb_ack_msg->set_sequence_number(0);

  auto s = heartbeat_handler_->HandleMessage(std::move(hb_ack));

  heartbeat_handler_->DisableHeartbeats();
  heartbeat_handler_->EnableHeartbeats();

  time_system_->Sleep(std::chrono::milliseconds(5 * 5000 + 1));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(3, nats_conn_->published_msgs().size());
  hb = nats_conn_->published_msgs()[2].heartbeat();
  EXPECT_EQ(1, hb.sequence_number());
  EXPECT_EQ(2, hb.update_info().schema().size());
  CheckFilterElements(hb.update_info().data(), {"pl/service"}, {"pl/another_service"});
}

TEST_F(HeartbeatMessageHandlerTest, HandleHeartbeatRelationUpdates) {
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto hb = nats_conn_->published_msgs()[0].heartbeat();
  EXPECT_EQ(0, hb.sequence_number());
  EXPECT_THAT(hb.update_info(), Partially(EqualsProto(kAgentUpdateInfoSchemaNoTablets)));

  time_system_->Sleep(std::chrono::milliseconds(5 * 4000));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  auto hb_ack = std::make_unique<messages::VizierMessage>();
  auto hb_ack_msg = hb_ack->mutable_heartbeat_ack();
  hb_ack_msg->set_sequence_number(0);

  auto s = heartbeat_handler_->HandleMessage(std::move(hb_ack));
  Relation relation2({types::TIME64NS, types::FLOAT64}, {"time_", "gauge"});
  RelationInfo relation_info2("relation2", /* id */ 1, "desc2", relation2);
  s = relation_info_manager_->AddRelationInfo(relation_info2);

  time_system_->Sleep(std::chrono::milliseconds(5 * 5000 + 1));
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
    time_system_ = std::make_unique<event::SimulatedTimeSystem>();
    api_ = std::make_unique<px::event::APIImpl>(time_system_.get());
    dispatcher_ = api_->AllocateDispatcher("manager");
    nats_conn_ = std::make_unique<FakeNATSConnector<px::vizier::messages::VizierMessage>>();

    agent_info_ = agent::Info{};
    agent_info_.capabilities.set_collects_data(true);

    heartbeat_nack_handler_ = std::make_unique<HeartbeatNackMessageHandler>(
        dispatcher_.get(), &agent_info_, nats_conn_.get(), [this]() -> Status {
          called_reregister_++;
          return Status::OK();
        });
  }

  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  std::unique_ptr<event::APIImpl> api_;
  std::unique_ptr<event::Dispatcher> dispatcher_;
  std::unique_ptr<HeartbeatNackMessageHandler> heartbeat_nack_handler_;
  std::unique_ptr<FakeNATSConnector<px::vizier::messages::VizierMessage>> nats_conn_;
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
}  // namespace px
