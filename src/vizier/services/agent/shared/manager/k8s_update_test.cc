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

#include <chrono>
#include <utility>
#include <vector>

#include "src/common/event/api_impl.h"
#include "src/common/event/nats.h"
#include "src/common/testing/event/simulated_time_system.h"
#include "src/common/testing/testing.h"
#include "src/vizier/services/agent/shared/manager/k8s_update.h"
#include "src/vizier/services/agent/shared/manager/test_utils.h"

namespace px {
namespace vizier {
namespace agent {

using ::px::testing::proto::EqualsProto;

constexpr char kUpdate0[] = R"(
  namespace_update {
    name: "pl"
    uid: "namespace_1"
    start_timestamp_ns: 999
  }
  update_version: $0
  prev_update_version: $1
)";

std::string TestResourceUpdate(int64_t curr_version, int64_t prev_version) {
  return absl::Substitute(kUpdate0, curr_version, prev_version);
}

std::unique_ptr<messages::VizierMessage> TestK8sUpdateMsg(const std::string& update) {
  auto sent_update = std::make_unique<messages::VizierMessage>();
  google::protobuf::TextFormat::MergeFromString(
      update, sent_update->mutable_k8s_metadata_message()->mutable_k8s_metadata_update());
  return sent_update;
}

std::unique_ptr<messages::VizierMessage> TestMissingUpdateResp(
    const std::vector<std::string> updates, int64_t earliest_version, int64_t latest_version) {
  auto sent_update = std::make_unique<messages::VizierMessage>();
  auto resp = sent_update->mutable_k8s_metadata_message()->mutable_missing_k8s_metadata_response();
  for (const auto& update_str : updates) {
    auto update = resp->add_updates();
    google::protobuf::TextFormat::MergeFromString(update_str, update);
  }
  resp->set_first_update_available(earliest_version);
  resp->set_last_update_available(latest_version);
  return sent_update;
}

class K8sUpdateHandlerTest : public ::testing::Test {
 protected:
  void TearDown() override { dispatcher_->Exit(); }

  K8sUpdateHandlerTest() {
    time_system_ = std::make_unique<event::SimulatedTimeSystem>();
    api_ = std::make_unique<px::event::APIImpl>(time_system_.get());
    dispatcher_ = api_->AllocateDispatcher("manager");
    nats_conn_ = std::make_unique<FakeNATSConnector<px::vizier::messages::VizierMessage>>();

    fake_mds_manager_ = std::make_unique<FakeAgentMetadataStateManager>();
    k8s_update_handler_ = std::make_unique<K8sUpdateHandler>(
        dispatcher_.get(), fake_mds_manager_.get(), &agent_info_, nats_conn_.get(), "all", 2);

    // Intentionally using nonconsecutive resource numbers.
    // 10, 20, 30, 40, 50, 60, 70
    updates_ = {TestResourceUpdate(10, 0),  TestResourceUpdate(20, 10), TestResourceUpdate(30, 20),
                TestResourceUpdate(40, 30), TestResourceUpdate(50, 40), TestResourceUpdate(60, 50),
                TestResourceUpdate(70, 60)};
  }

  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
  std::unique_ptr<event::APIImpl> api_;
  std::unique_ptr<event::Dispatcher> dispatcher_;
  std::unique_ptr<FakeAgentMetadataStateManager> fake_mds_manager_;
  std::unique_ptr<K8sUpdateHandler> k8s_update_handler_;
  std::unique_ptr<FakeNATSConnector<px::vizier::messages::VizierMessage>> nats_conn_;
  agent::Info agent_info_;
  std::filesystem::path proc_path_;
  std::filesystem::path sysfs_path_;
  std::vector<std::string> updates_;
};

TEST_F(K8sUpdateHandlerTest, HandleK8sUpdate) {
  auto update = TestResourceUpdate(1, 0);
  auto sent_update = TestK8sUpdateMsg(update);
  auto s = k8s_update_handler_->HandleMessage(std::move(sent_update));
  EXPECT_OK(s);
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());
  auto recv_update = fake_mds_manager_->k8s_update(0);
  EXPECT_THAT(*recv_update, EqualsProto(update));
}

TEST_F(K8sUpdateHandlerTest, RequestInitialK8sUpdates) {
  // Check that the handler automatically requests the backlog of updates.
  EXPECT_EQ(0, nats_conn_->published_msgs().size());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(0));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto req = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  auto missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(0, missing_md.from_update_version());
  EXPECT_EQ(0, missing_md.to_update_version());

  // No requests in backlog.
  auto sent_missing = TestMissingUpdateResp({}, 0, 0);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_missing)));
  EXPECT_EQ(0, fake_mds_manager_->num_k8s_updates());

  // New updates go through.
  auto sent_update = TestK8sUpdateMsg(updates_[0]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());
}

TEST_F(K8sUpdateHandlerTest, RequestInitialK8sUpdatesTimeout) {
  // Check that the handler automatically requests the backlog of updates.
  EXPECT_EQ(0, nats_conn_->published_msgs().size());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(0));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto req = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  auto missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(0, missing_md.from_update_version());
  EXPECT_EQ(0, missing_md.to_update_version());

  // Push forward the clock so that the new request gets sent.
  time_system_->Sleep(std::chrono::seconds(10));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(2, nats_conn_->published_msgs().size());
  req = nats_conn_->published_msgs()[1];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(0, missing_md.from_update_version());
  EXPECT_EQ(0, missing_md.to_update_version());
}

TEST_F(K8sUpdateHandlerTest, RequestInitialK8sUpdatesTimeoutMiddleMsg) {
  // Check that the handler automatically requests the backlog of updates.
  EXPECT_EQ(0, nats_conn_->published_msgs().size());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(0));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto req = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  auto missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(0, missing_md.from_update_version());
  EXPECT_EQ(0, missing_md.to_update_version());

  // Send the third message
  auto third = TestMissingUpdateResp({updates_[2]}, 10, 30);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(third)));
  EXPECT_EQ(0, fake_mds_manager_->num_k8s_updates());

  // Send the first message
  auto first = TestMissingUpdateResp({updates_[0]}, 10, 30);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(first)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(6));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  EXPECT_EQ(2, nats_conn_->published_msgs().size());
  req = nats_conn_->published_msgs()[1];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(11, missing_md.from_update_version());
  EXPECT_EQ(30, missing_md.to_update_version());
}

TEST_F(K8sUpdateHandlerTest, RequestMissingK8sUpdates) {
  // Check flow for re-requesting missing updates.
  // Makes sure that missing updates paginated across multiple responses work.
  // Makes sure that updates that come in before the backlog is resolved get
  // added to the backlog.
  // Makes sure that unnecessary requests to NATS don't get made.
  // Makes sure that the updates are applied in the correct order.

  // Send update 0
  auto sent_update = TestK8sUpdateMsg(updates_[0]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Send update 4
  sent_update = TestK8sUpdateMsg(updates_[4]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(0));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check that NATS has received a missing metadata request.
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto req = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  auto missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(11, missing_md.from_update_version());
  EXPECT_EQ(50, missing_md.to_update_version());

  // Missing MD response part 1/2
  auto sent_missing = TestMissingUpdateResp({updates_[0], updates_[1], updates_[2]}, 10, 40);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_missing)));
  EXPECT_EQ(3, fake_mds_manager_->num_k8s_updates());

  // Send update 5. Should go to backlog.
  sent_update = TestK8sUpdateMsg(updates_[5]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(3, fake_mds_manager_->num_k8s_updates());

  // Missing Metadata response part 2/2. Backlog gets written.
  sent_missing = TestMissingUpdateResp({updates_[3]}, 10, 40);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_missing)));
  // Check that backlog is flushed.
  EXPECT_EQ(6, fake_mds_manager_->num_k8s_updates());

  // Update 6 goes through now that we have resolved the backlog.
  sent_update = TestK8sUpdateMsg(updates_[6]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(7, fake_mds_manager_->num_k8s_updates());

  // Check that no more re-request messages were sent to NATS.
  time_system_->Sleep(std::chrono::seconds(6));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());

  // Check that the messages were sent in the right order.
  for (const auto& [i, expected_update] : Enumerate(updates_)) {
    EXPECT_THAT(*(fake_mds_manager_->k8s_update(i)), EqualsProto(expected_update));
  }
}

TEST_F(K8sUpdateHandlerTest, RequestMissingK8sUpdatesTimeoutRerequest) {
  // Check for handling of timeout after rerequesting missing K8s updates.

  // Send update 0
  auto sent_update = TestK8sUpdateMsg(updates_[0]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Send update 0
  sent_update = TestK8sUpdateMsg(updates_[4]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(0));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check that NATS has received a missing metadata request.
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto req = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  auto missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(11, missing_md.from_update_version());
  EXPECT_EQ(50, missing_md.to_update_version());

  // Push forward the clock so that the re-request gets sent.
  time_system_->Sleep(std::chrono::seconds(6));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check that the re-request got sent.
  EXPECT_EQ(2, nats_conn_->published_msgs().size());
  req = nats_conn_->published_msgs()[1];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(11, missing_md.from_update_version());
  EXPECT_EQ(50, missing_md.to_update_version());
}

TEST_F(K8sUpdateHandlerTest, RequestMissingK8sUpdatesMDSTruncated) {
  // Check that when the metadata service says it is missing a chunk of updates,
  // the k8s update handler can tolerate that condition and proceed forward.

  // Send update 0
  auto sent_update = TestK8sUpdateMsg(updates_[0]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Send update 4
  sent_update = TestK8sUpdateMsg(updates_[4]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(0));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check that NATS has received a missing metadata request.
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto req = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  auto missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(11, missing_md.from_update_version());
  EXPECT_EQ(50, missing_md.to_update_version());

  // Missing MD response part 1/1
  auto sent_missing = TestMissingUpdateResp({updates_[2], updates_[3]}, 30, 40);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_missing)));
  EXPECT_EQ(4, fake_mds_manager_->num_k8s_updates());

  // Check that no more re-request messages were sent to NATS.
  time_system_->Sleep(std::chrono::seconds(6));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());

  // Check that the messages were sent in the right order.
  std::vector<std::string> expected_updates = {updates_[0], updates_[2], updates_[3], updates_[4]};
  for (const auto& [i, expected_update] : Enumerate(expected_updates)) {
    EXPECT_THAT(*(fake_mds_manager_->k8s_update(i)), EqualsProto(expected_update));
  }
}

TEST_F(K8sUpdateHandlerTest, RequestMissingK8sUpdatesMDSTruncatedZeroResp) {
  // Check that when the metadata service says it is missing a chunk of updates,
  // (and doesn't send any updates in the requested range because they are too
  // far back) the k8s update handler can tolerate that condition and proceed forward.

  // Send update 0
  auto sent_update = TestK8sUpdateMsg(updates_[0]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Send update 4
  sent_update = TestK8sUpdateMsg(updates_[4]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(0));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check that NATS has received a missing metadata request.
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto req = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  auto missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(11, missing_md.from_update_version());
  EXPECT_EQ(50, missing_md.to_update_version());

  // Missing MD response part 1/1
  auto sent_missing = TestMissingUpdateResp({}, 50, 50);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_missing)));
  EXPECT_EQ(2, fake_mds_manager_->num_k8s_updates());

  // Check that no more re-request messages were sent to NATS.
  time_system_->Sleep(std::chrono::seconds(6));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, nats_conn_->published_msgs().size());

  // Check that the messages were sent in the right order.
  std::vector<std::string> expected_updates = {updates_[0], updates_[4]};
  for (const auto& [i, expected_update] : Enumerate(expected_updates)) {
    EXPECT_THAT(*(fake_mds_manager_->k8s_update(i)), EqualsProto(expected_update));
  }
}

TEST_F(K8sUpdateHandlerTest, RequestMissingK8sUpdatesDropBacklog) {
  // Request missing k8s updates, adding new items to backlog to be flushed,
  // but then they need to be rerequested.

  // Send update 0
  auto sent_update = TestK8sUpdateMsg(updates_[0]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Send update 2
  sent_update = TestK8sUpdateMsg(updates_[2]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  // Send update 3
  sent_update = TestK8sUpdateMsg(updates_[3]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  // Last 2 should be in the backlog.
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Push forward the clock so that the request gets sent.
  time_system_->Sleep(std::chrono::seconds(0));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check that NATS has received a missing metadata request.
  EXPECT_EQ(1, nats_conn_->published_msgs().size());
  auto req = nats_conn_->published_msgs()[0];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  auto missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(11, missing_md.from_update_version());
  EXPECT_EQ(30, missing_md.to_update_version());

  // Send update 4. This should push update 2 (30) out of the backlog.
  // We will need to rerequest it.
  sent_update = TestK8sUpdateMsg(updates_[4]);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_update)));
  // Last 3 should be in the backlog.
  EXPECT_EQ(1, fake_mds_manager_->num_k8s_updates());

  // Missing MD response part 1/1, req 1
  auto sent_missing = TestMissingUpdateResp({updates_[0], updates_[1]}, 10, 20);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_missing)));
  // Update 2 (30) shouldn't have been written yet, it should have been dropped..
  // We only have updates 0 and 1 due to the backlog capacity.
  EXPECT_EQ(2, fake_mds_manager_->num_k8s_updates());

  // Push forward the clock so that the 2nd dropped backlog request gets sent.
  time_system_->Sleep(std::chrono::seconds(6));
  dispatcher_->Run(event::Dispatcher::RunType::NonBlock);

  // Check that NATS has received a missing metadata request.
  EXPECT_EQ(2, nats_conn_->published_msgs().size());
  req = nats_conn_->published_msgs()[1];
  EXPECT_TRUE(req.k8s_metadata_message().has_missing_k8s_metadata_request());
  missing_md = req.k8s_metadata_message().missing_k8s_metadata_request();
  EXPECT_EQ("all", missing_md.selector());
  EXPECT_EQ(21, missing_md.from_update_version());
  EXPECT_EQ(40, missing_md.to_update_version());

  // Missing MD response part 1/1, req 2
  sent_missing = TestMissingUpdateResp({updates_[1], updates_[2]}, 20, 30);
  ASSERT_OK(k8s_update_handler_->HandleMessage(std::move(sent_missing)));

  // Now all 5 updates should be received.
  EXPECT_EQ(5, fake_mds_manager_->num_k8s_updates());

  // Check that the messages were sent in the right order.
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_THAT(*(fake_mds_manager_->k8s_update(i)), EqualsProto(updates_[i]));
  }
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
