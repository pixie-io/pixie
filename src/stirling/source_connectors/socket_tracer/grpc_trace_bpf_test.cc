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
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <thread>

#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/grpc.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/greeter_server.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto/greet.grpc.pb.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::grpc::Channel;
using ::px::stirling::grpc::kGRPCMessageHeaderSizeInBytes;
using ::px::stirling::protocols::http2::testing::HelloReply;
using ::px::stirling::protocols::http2::testing::HelloRequest;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::testing::proto::EqualsProto;
using ::px::types::ColumnWrapperRecordBatch;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

HelloReply GetHelloReply(const ColumnWrapperRecordBatch& record_batch, const size_t idx) {
  HelloReply received_reply;
  std::string msg = record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(idx);
  if (!msg.empty()) {
    received_reply.ParseFromString(msg.substr(kGRPCMessageHeaderSizeInBytes));
  }
  return received_reply;
}

HelloRequest GetHelloRequest(const ColumnWrapperRecordBatch& record_batch, const size_t idx) {
  HelloRequest received_reply;
  std::string msg = record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(idx);
  if (!msg.empty()) {
    received_reply.ParseFromString(msg.substr(kGRPCMessageHeaderSizeInBytes));
  }
  return received_reply;
}

class GRPCServer {
 public:
  static constexpr std::string_view kServerPath =
      "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server/"
      "golang_$0_grpc_server";

  GRPCServer() = default;

  void LaunchServer(std::string go_version, bool use_https) {
    std::string server_path = absl::Substitute(kServerPath, go_version);
    server_path = px::testing::BazelBinTestFilePath(server_path).string();
    PL_CHECK_OK(fs::Exists(server_path));

    std::string https_flag = use_https ? "--https=true" : "--https=false";
    PL_CHECK_OK(s_.Start({server_path, https_flag}));
    LOG(INFO) << "Server PID: " << s_.child_pid();

    // Give some time for the server to start up.
    sleep(2);

    std::string port_str;
    PL_CHECK_OK(s_.Stdout(&port_str));
    CHECK(absl::SimpleAtoi(port_str, &port_));
    CHECK_NE(0, port_);
  }

  int port() { return port_; }
  int pid() { return s_.child_pid(); }

  SubProcess s_;
  int port_ = -1;
};

class GRPCClient {
 public:
  static constexpr std::string_view kClientPath =
      "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_client/"
      "golang_$0_grpc_client";

  void LaunchClient(std::string_view go_version, bool use_https, int port) {
    std::string client_path = absl::Substitute(kClientPath, go_version);
    client_path = px::testing::BazelBinTestFilePath(client_path).string();

    PL_CHECK_OK(fs::Exists(client_path));

    const std::string https_flag = use_https ? "--https=true" : "--https=false";
    PL_CHECK_OK(c_.Start({client_path, https_flag, "-once", "-name=PixieLabs",
                          absl::StrCat("-address=localhost:", port)}));
    LOG(INFO) << "Client PID: " << c_.child_pid();
    CHECK_EQ(0, c_.Wait());
  }

  SubProcess c_;
};

struct TestParams {
  std::string go_version;
  bool use_https;
};

class GRPCTraceTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false>,
                      public ::testing::WithParamInterface<TestParams> {
 protected:
  GRPCTraceTest() {}

  void TearDown() override {
    server_.s_.Kill();
    CHECK_EQ(9, server_.s_.Wait()) << "Server should have been killed.";
  }

  GRPCServer server_;
  GRPCClient client_;
};

TEST_P(GRPCTraceTest, CaptureRPCTraceRecord) {
  auto params = GetParam();

  server_.LaunchServer(params.go_version, params.use_https);

  // Deploy uprobes on the newly launched server.
  RefreshContext(/* blocking_deploy_uprobes */ true);

  StartTransferDataThread();

  client_.LaunchClient(params.go_version, params.use_https, server_.port());

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_FALSE(tablets.empty());
  const types::ColumnWrapperRecordBatch& rb = tablets[0].records;
  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(rb, kHTTPUPIDIdx, server_.pid());
  ASSERT_GE(target_record_indices.size(), 1);

  // We should get exactly one record.
  const size_t idx = target_record_indices.front();
  const std::string scheme_text = params.use_https ? R"(":scheme":"https")" : R"(":scheme":"http")";

  md::UPID upid(rb[kHTTPUPIDIdx]->Get<types::UInt128Value>(idx).val);
  std::filesystem::path proc_pid_path =
      system::Config::GetInstance().proc_path() / std::to_string(server_.pid());
  ASSERT_OK_AND_ASSIGN(int64_t pid_start_time, system::GetPIDStartTimeTicks(proc_pid_path));
  md::UPID expected_upid(/* asid */ 0, server_.pid(), pid_start_time);
  EXPECT_EQ(upid, expected_upid);

  EXPECT_THAT(
      std::string(rb[kHTTPReqHeadersIdx]->Get<types::StringValue>(idx)),
      AllOf(HasSubstr(absl::Substitute(R"(":authority":"localhost:$0")", server_.port())),
            HasSubstr(R"(":method":"POST")"), HasSubstr(scheme_text),
            HasSubstr(absl::StrCat(R"(":scheme":)", params.use_https ? R"("https")" : R"("http")")),
            HasSubstr(R"("content-type":"application/grpc")"), HasSubstr(R"("grpc-timeout")"),
            HasSubstr(R"("te":"trailers","user-agent")")));
  EXPECT_THAT(
      std::string(rb[kHTTPRespHeadersIdx]->Get<types::StringValue>(idx)),
      AllOf(HasSubstr(R"(":status":"200")"), HasSubstr(R"("content-type":"application/grpc")"),
            HasSubstr(R"("grpc-message":"")"), HasSubstr(R"("grpc-status":"0"})")));
  EXPECT_THAT(std::string(rb[kHTTPRemoteAddrIdx]->Get<types::StringValue>(idx)),
              AnyOf(HasSubstr("127.0.0.1"), HasSubstr("::1")));
  EXPECT_EQ(2, rb[kHTTPMajorVersionIdx]->Get<types::Int64Value>(idx).val);
  EXPECT_EQ(0, rb[kHTTPMinorVersionIdx]->Get<types::Int64Value>(idx).val);
  EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
            rb[kHTTPContentTypeIdx]->Get<types::Int64Value>(idx).val);

  EXPECT_EQ(rb[kHTTPRespBodyIdx]->Get<types::StringValue>(idx).string(), R"(1: "Hello PixieLabs")");
}

INSTANTIATE_TEST_SUITE_P(SecurityModeTest, GRPCTraceTest,
                         ::testing::Values(TestParams{"1_16", true}, TestParams{"1_16", false},
                                           TestParams{"1_17", true}, TestParams{"1_17", false}));

}  // namespace stirling
}  // namespace px
