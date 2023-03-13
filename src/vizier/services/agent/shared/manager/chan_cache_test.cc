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

#include <absl/strings/substitute.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <gtest/gtest.h>

#include "src/carnot/carnotpb/carnot.grpc.pb.h"
#include "src/common/base/base.h"
#include "src/common/testing/grpc_utils/grpc_utils.h"
#include "src/common/testing/testing.h"
#include "src/vizier/services/agent/shared/manager/chan_cache.h"

namespace px {
namespace vizier {
namespace agent {

using grpc::Channel;
using grpc::InsecureChannelCredentials;
using grpc::InsecureServerCredentials;
using grpc::Server;
using grpc::ServerBuilder;

class FakeGRPCRouter final : public carnotpb::ResultSinkService::Service {
 public:
  /**
   * TransferResultChunk implements the RPC method.
   */
  ::grpc::Status TransferResultChunk(
      ::grpc::ServerContext*, ::grpc::ServerReader<::px::carnotpb::TransferResultChunkRequest>*,
      ::px::carnotpb::TransferResultChunkResponse*) override {
    return ::grpc::Status::OK;
  }
};

class ChanCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup server.
    service_ = std::make_unique<FakeGRPCRouter>();
    ServerBuilder builder;
    builder.AddListeningPort(GetServerAddress(), InsecureServerCredentials(), &port_);
    builder.RegisterService(service_.get());
    server_ = builder.BuildAndStart();
  }

  void RunRPC(carnotpb::ResultSinkService::Stub* stub) { RunTransferResultChunk(stub); }

  void RunTransferResultChunk(carnotpb::ResultSinkService::Stub* stub) {
    grpc::ClientContext context;
    carnotpb::TransferResultChunkRequest request;
    carnotpb::TransferResultChunkResponse response;

    auto writer = stub->TransferResultChunk(&context, &response);
    writer->Write(request);
    writer->WritesDone();
    auto s = writer->Finish();
    if (!s.ok()) {
      LOG(ERROR) << "Error: " << s.error_message();
    }
  }

  void TearDown() override { server_->Shutdown(); }

  std::string GetServerAddress() { return absl::Substitute("$0:$1", hostname, port_); }

  std::unique_ptr<carnotpb::ResultSinkService::Stub> BuildStub(
      const std::shared_ptr<Channel>& channel) {
    return carnotpb::ResultSinkService::NewStub(channel);
  }

  std::unique_ptr<FakeGRPCRouter> service_;

  std::unique_ptr<Server> server_;
  std::string hostname = "0.0.0.0";
  int port_ = 0;
};

TEST_F(ChanCacheTest, add_and_get_and_gc_with_no_changes) {
  // Before we add anything, the chan_cache shouldn't return anything for the address.
  ChanCache chan_cache(std::chrono::minutes(5));
  EXPECT_EQ(chan_cache.GetChan(GetServerAddress()), nullptr);

  // Create channel to wrong address.
  auto channel = grpc::CreateChannel(GetServerAddress(), InsecureChannelCredentials());

  // Add the channel to the cache.
  chan_cache.Add(GetServerAddress(), channel);

  // We dont' do anything with the channel so it should be idle.
  EXPECT_EQ(channel->GetState(false), grpc_connectivity_state::GRPC_CHANNEL_IDLE);
  EXPECT_EQ(chan_cache.GetChan(GetServerAddress()), channel);

  // Garbage collect shouldn't delete the connection because we are still in the grace period.
  EXPECT_OK(chan_cache.CleanupChans());
  EXPECT_EQ(chan_cache.GetChan(GetServerAddress()), channel);
}

TEST_F(ChanCacheTest, gc_on_failing_conn) {
  // Before we add anything, the chan_cache shouldn't return anything for the address.
  ChanCache chan_cache(std::chrono::minutes(5));
  auto wrong_server_address = absl::Substitute("$0:$1", hostname, port_ + 1);
  EXPECT_EQ(chan_cache.GetChan(wrong_server_address), nullptr);

  // Create channel to wrong address.
  auto channel = grpc::CreateChannel(wrong_server_address, InsecureChannelCredentials());

  // Add the channel to the cache.
  chan_cache.Add(wrong_server_address, channel);

  // The channel will be idle until we attempt a connection.
  EXPECT_EQ(channel->GetState(false), grpc_connectivity_state::GRPC_CHANNEL_IDLE);

  // Attempt a connection using a stub
  auto stub = BuildStub(channel);
  RunRPC(stub.get());

  // State should be failing.
  EXPECT_EQ(channel->GetState(false), grpc_connectivity_state::GRPC_CHANNEL_TRANSIENT_FAILURE);

  // Garbage collector has not run yet, so channel should still be findable.
  EXPECT_EQ(chan_cache.GetChan(wrong_server_address), channel);

  // Garbage Collector should rm the channel.
  EXPECT_OK(chan_cache.CleanupChans());
  EXPECT_EQ(chan_cache.GetChan(wrong_server_address), nullptr);
}

TEST_F(ChanCacheTest, channel_idleness_triggers_gc) {
  ChanCache chan_cache(std::chrono::milliseconds(500));
  EXPECT_EQ(chan_cache.GetChan(GetServerAddress()), nullptr);
  // Set max idle time and build the channel.
  grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_CLIENT_IDLE_TIMEOUT_MS, 1000);
  auto channel =
      ::grpc::CreateCustomChannel(GetServerAddress(), InsecureChannelCredentials(), args);
  auto stub = BuildStub(channel);

  chan_cache.Add(GetServerAddress(), channel);
  // The initial channel state should be IDLE.
  EXPECT_EQ(channel->GetState(false), GRPC_CHANNEL_IDLE);
  // After sending RPC, channel state should be READY.
  RunRPC(stub.get());
  EXPECT_EQ(channel->GetState(false), GRPC_CHANNEL_READY);
  // After a period time not using the channel, the channel state should switch
  // to IDLE.
  TestSleep(1200);
  EXPECT_EQ(channel->GetState(false), GRPC_CHANNEL_IDLE);

  // Garbage Collection should kill the channel, because the channel is idle and it's been alive
  // longer than the grace period.
  EXPECT_OK(chan_cache.CleanupChans());
  EXPECT_EQ(chan_cache.GetChan(GetServerAddress()), nullptr);
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
