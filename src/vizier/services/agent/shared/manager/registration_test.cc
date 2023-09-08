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

#include <utility>
#include <vector>

#include "src/common/event/api_impl.h"
#include "src/common/event/libuv.h"
#include "src/common/event/nats.h"
#include "src/common/testing/event/simulated_time_system.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/vizier/messages/messagespb/messages.pb.h"
#include "src/vizier/services/agent/shared/manager/manager.h"
#include "src/vizier/services/agent/shared/manager/registration.h"
#include "src/vizier/services/agent/shared/manager/test_utils.h"

#include "src/common/testing/testing.h"

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

class RegistrationHandlerTest : public ::testing::Test {
 protected:
  void TearDown() override { dispatcher_->Exit(); }

  RegistrationHandlerTest() {
    time_system_ = std::make_unique<event::SimulatedTimeSystem>();
    api_ = std::make_unique<px::event::APIImpl>(time_system_.get());
    dispatcher_ = api_->AllocateDispatcher("manager");
    nats_conn_ = std::make_unique<FakeNATSConnector<px::vizier::messages::VizierMessage>>();

    agent_info_ = agent::Info{};
    agent_info_.agent_id = sole::uuid4();
    agent_info_.hostname = "hostname";
    agent_info_.address = "address";
    agent_info_.pod_name = "pod_name";
    agent_info_.host_ip = "host_ip";
    agent_info_.capabilities.set_collects_data(true);
    agent_info_.kernel_version =
        system::ParseKernelVersionString("5.15.0-106-generic").ValueOrDie();

    auto register_hook = [this](uint32_t asid) -> Status {
      called_register_++;
      agent_info_.asid = asid;
      register_asid_ = asid;
      return Status::OK();
    };

    auto reregister_hook = [this](uint32_t asid) -> Status {
      called_reregister_++;
      reregister_asid_ = asid;
      return Status::OK();
    };

    registration_handler_ = std::make_unique<RegistrationHandler>(
        dispatcher_.get(), &agent_info_, nats_conn_.get(), register_hook, reregister_hook);
  }

  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  std::unique_ptr<event::APIImpl> api_;
  std::unique_ptr<event::Dispatcher> dispatcher_;
  std::unique_ptr<RegistrationHandler> registration_handler_;
  std::unique_ptr<FakeNATSConnector<px::vizier::messages::VizierMessage>> nats_conn_;
  agent::Info agent_info_;
  int32_t called_register_ = 0;
  int32_t called_reregister_ = 0;
  uint32_t register_asid_ = 0;
  uint32_t reregister_asid_ = 0;
};

TEST_F(RegistrationHandlerTest, RegisterAgent) {
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  registration_handler_->RegisterAgent();

  // Advance the clock to account for the random wait time.
  time_system_->Sleep(std::chrono::milliseconds(60 * 1000));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());

  // Check contents of registration msg.
  auto msg = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(msg.has_register_agent_request());
  auto req = msg.register_agent_request();
  EXPECT_TRUE(req.info().capabilities().collects_data());
  auto uuid = ParseUUID(req.info().agent_id()).ConsumeValueOrDie();
  EXPECT_EQ(agent_info_.agent_id, uuid);
  EXPECT_EQ(agent_info_.address, req.info().ip_address());
  EXPECT_EQ(agent_info_.hostname, req.info().host_info().hostname());
  EXPECT_EQ(agent_info_.pod_name, req.info().host_info().pod_name());
  EXPECT_EQ(agent_info_.host_ip, req.info().host_info().host_ip());
  EXPECT_EQ(agent_info_.kernel_version.version, req.info().host_info().kernel().version());
  EXPECT_EQ(agent_info_.kernel_version.major_rev, req.info().host_info().kernel().major_rev());
  EXPECT_EQ(agent_info_.kernel_version.minor_rev, req.info().host_info().kernel().minor_rev());

  auto registration_ack = std::make_unique<messages::VizierMessage>();
  registration_ack->mutable_register_agent_response()->set_asid(10);

  EXPECT_OK(registration_handler_->HandleMessage(std::move(registration_ack)));

  EXPECT_EQ(1, called_register_);
  EXPECT_EQ(10, register_asid_);
}

TEST_F(RegistrationHandlerTest, RegisterAndReregisterAgent) {
  // Agent registration setup
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  registration_handler_->RegisterAgent();
  time_system_->Sleep(std::chrono::milliseconds(60 * 1000 + 1));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto registration_ack = std::make_unique<messages::VizierMessage>();
  registration_ack->mutable_register_agent_response()->set_asid(10);
  EXPECT_OK(registration_handler_->HandleMessage(std::move(registration_ack)));
  EXPECT_EQ(1, called_register_);
  EXPECT_EQ(10, register_asid_);

  // Now reregister the agent.
  registration_handler_->ReregisterAgent();
  time_system_->Sleep(std::chrono::milliseconds(120 * 1000 + 1));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(2, nats_conn_->published_msgs().size());

  // Check that the ASID got sent again.
  auto msg = nats_conn_->published_msgs()[1];
  EXPECT_TRUE(msg.has_register_agent_request());
  EXPECT_EQ(10, msg.register_agent_request().asid());

  auto reregistration_ack = std::make_unique<messages::VizierMessage>();
  reregistration_ack->mutable_register_agent_response()->set_asid(10);

  EXPECT_OK(registration_handler_->HandleMessage(std::move(reregistration_ack)));
  EXPECT_EQ(1, called_reregister_);
  EXPECT_EQ(10, reregister_asid_);
}

TEST_F(RegistrationHandlerTest, RegisterAgentTimeout) {
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  registration_handler_->RegisterAgent();

  // Advance the clock to account for the random wait time.
  time_system_->Sleep(std::chrono::milliseconds(60 * 1000));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());

  time_system_->Sleep(std::chrono::milliseconds(120 * 1000));
  ASSERT_DEATH(dispatcher_->Run(event::Dispatcher::RunType::NonBlock),
               "Timeout waiting for registration ack");
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
