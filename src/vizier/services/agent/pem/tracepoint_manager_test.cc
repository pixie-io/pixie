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
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "src/common/event/event.h"
#include "src/common/event/nats.h"
#include "src/common/testing/event/simulated_time_system.h"
#include "src/common/testing/protobuf.h"

#include "src/common/testing/testing.h"
#include "src/stirling/testing/stirling_mock.h"
#include "src/vizier/services/agent/pem/tracepoint_manager.h"
#include "src/vizier/services/agent/shared/manager/test_utils.h"

namespace px {
namespace vizier {
namespace agent {

using ::google::protobuf::TextFormat;
using stirling::stirlingpb::InfoClass;
using stirling::stirlingpb::Publish;
using ::testing::_;
using ::testing::Return;

const char* kInfoClass0 = R"(
  schema {
    name: "cpu"
    elements {
      name: "user_percentage"
      type: FLOAT64
      stype: ST_NONE
      ptype: METRIC_GAUGE
      desc: "User percentage"
    }
    tabletized: false
    tabletization_key: 18446744073709551615
  }
)";

class TracepointManagerTest : public ::testing::Test {
 protected:
  void TearDown() override { dispatcher_->Exit(); }

  TracepointManagerTest() {
    time_system_ = std::make_unique<event::SimulatedTimeSystem>();
    api_ = std::make_unique<px::event::APIImpl>(time_system_.get());
    dispatcher_ = api_->AllocateDispatcher("manager");
    nats_conn_ = std::make_unique<FakeNATSConnector<px::vizier::messages::VizierMessage>>();

    relation_info_manager_ = std::make_unique<RelationInfoManager>();
    agent_info_ = agent::Info{};
    agent_info_.capabilities.set_collects_data(true);

    tracepoint_manager_ = std::make_unique<TracepointManager>(
        dispatcher_.get(), &agent_info_, nats_conn_.get(), &stirling_, &table_store_,
        relation_info_manager_.get());
  }

  Publish createTestPublishMsg() {
    Publish expected_publish_pb;
    auto* info_class = expected_publish_pb.add_published_info_classes();
    CHECK(TextFormat::MergeFromString(kInfoClass0, info_class));
    info_class->set_id(0);
    return expected_publish_pb;
  }

  messages::TracepointInfoUpdate extractTracepointInfoUpdate(const messages::VizierMessage& msg) {
    CHECK(msg.has_tracepoint_message());
    CHECK(msg.tracepoint_message().has_tracepoint_info_update());
    return msg.tracepoint_message().tracepoint_info_update();
  }

  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  std::unique_ptr<event::APIImpl> api_;
  std::unique_ptr<event::Dispatcher> dispatcher_;
  std::unique_ptr<RelationInfoManager> relation_info_manager_;
  std::unique_ptr<TracepointManager> tracepoint_manager_;
  std::unique_ptr<FakeNATSConnector<px::vizier::messages::VizierMessage>> nats_conn_;
  table_store::TableStore table_store_;
  agent::Info agent_info_;
  stirling::MockStirling stirling_;
};

TEST_F(TracepointManagerTest, BadMessage) {
  auto msg = std::make_unique<px::vizier::messages::VizierMessage>();
  msg->mutable_heartbeat_ack()->set_sequence_number(0);
  EXPECT_NOT_OK(tracepoint_manager_->HandleMessage(std::move(msg)));
}

TEST_F(TracepointManagerTest, CreateTracepoint) {
  auto msg = std::make_unique<px::vizier::messages::VizierMessage>();
  auto* tracepoint_req = msg->mutable_tracepoint_message()->mutable_register_tracepoint_request();
  sole::uuid tracepoint_id = sole::uuid4();
  ToProto(tracepoint_id, tracepoint_req->mutable_id());
  auto* tracepoint = tracepoint_req->mutable_tracepoint_deployment();
  tracepoint->set_name("test_tracepoint");

  EXPECT_CALL(stirling_,
              RegisterTracepoint(tracepoint_id, ::testing::Pointee(testing::proto::EqualsProto(
                                                    tracepoint->DebugString()))));
  EXPECT_OK(tracepoint_manager_->HandleMessage(std::move(msg)));

  EXPECT_CALL(stirling_, GetTracepointInfo(tracepoint_id))
      .WillOnce(Return(error::ResourceUnavailable("Not ready yet")));

  time_system_->Sleep(std::chrono::seconds(5));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check to make sure no updates were sent to MDS.
  EXPECT_EQ(0, nats_conn_->published_msgs().size());

  // In the next step we will make Stirling return a valid proto.
  // This should cause the publications of the updated state.
  Publish expected_publish_pb = createTestPublishMsg();
  EXPECT_CALL(stirling_, GetTracepointInfo(tracepoint_id))
      .WillRepeatedly(Return(expected_publish_pb));

  time_system_->Sleep(std::chrono::seconds(10));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  ASSERT_EQ(1, nats_conn_->published_msgs().size());
  auto update = extractTracepointInfoUpdate(nats_conn_->published_msgs()[0]);
  EXPECT_EQ(statuspb::RUNNING_STATE, update.state());
  EXPECT_TRUE(relation_info_manager_->HasRelation("cpu"));
}

TEST_F(TracepointManagerTest, CreateTracepointFailed) {
  auto msg = std::make_unique<px::vizier::messages::VizierMessage>();
  auto* tracepoint_req = msg->mutable_tracepoint_message()->mutable_register_tracepoint_request();
  sole::uuid tracepoint_id = sole::uuid4();
  ToProto(tracepoint_id, tracepoint_req->mutable_id());
  auto* tracepoint = tracepoint_req->mutable_tracepoint_deployment();
  tracepoint->set_name("test_tracepoint");

  EXPECT_CALL(stirling_,
              RegisterTracepoint(tracepoint_id, ::testing::Pointee(testing::proto::EqualsProto(
                                                    tracepoint->DebugString()))));
  EXPECT_OK(tracepoint_manager_->HandleMessage(std::move(msg)));

  EXPECT_CALL(stirling_, GetTracepointInfo(tracepoint_id))
      .WillOnce(Return(error::ResourceUnavailable("Not ready yet")));

  time_system_->Sleep(std::chrono::seconds(5));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check to make sure no updates were sent to MDS.
  EXPECT_EQ(0, nats_conn_->published_msgs().size());
  EXPECT_CALL(stirling_, GetTracepointInfo(tracepoint_id))
      .WillRepeatedly(Return(error::Internal("Probe failed for unknown reasons")));

  time_system_->Sleep(std::chrono::seconds(10));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  ASSERT_EQ(1, nats_conn_->published_msgs().size());
  auto update = extractTracepointInfoUpdate(nats_conn_->published_msgs()[0]);
  EXPECT_EQ(statuspb::FAILED_STATE, update.state());
}

TEST_F(TracepointManagerTest, CreateTracepointPreconditionFailed) {
  // Precondition failed happens when the binary does not exist on this machine.
  auto msg = std::make_unique<px::vizier::messages::VizierMessage>();
  auto* tracepoint_req = msg->mutable_tracepoint_message()->mutable_register_tracepoint_request();
  sole::uuid tracepoint_id = sole::uuid4();
  ToProto(tracepoint_id, tracepoint_req->mutable_id());
  auto* tracepoint = tracepoint_req->mutable_tracepoint_deployment();
  tracepoint->set_name("test_tracepoint");

  EXPECT_CALL(stirling_,
              RegisterTracepoint(tracepoint_id, ::testing::Pointee(testing::proto::EqualsProto(
                                                    tracepoint->DebugString()))));
  EXPECT_OK(tracepoint_manager_->HandleMessage(std::move(msg)));

  EXPECT_CALL(stirling_, GetTracepointInfo(tracepoint_id))
      .WillOnce(Return(error::ResourceUnavailable("Not ready yet")));

  time_system_->Sleep(std::chrono::seconds(5));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check to make sure no updates were sent to MDS.
  EXPECT_EQ(0, nats_conn_->published_msgs().size());
  EXPECT_CALL(stirling_, GetTracepointInfo(tracepoint_id))
      .WillRepeatedly(Return(error::FailedPrecondition("nothing to see here")));

  time_system_->Sleep(std::chrono::seconds(10));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  ASSERT_EQ(1, nats_conn_->published_msgs().size());
  auto update = extractTracepointInfoUpdate(nats_conn_->published_msgs()[0]);
  EXPECT_EQ(statuspb::FAILED_STATE, update.state());
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
