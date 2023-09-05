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

#include <chrono>
#include <filesystem>
#include <thread>

#include "src/common/exec/subprocess.h"
#include "src/common/system/kernel_version.h"
#include "src/common/system/proc_pid_path.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/grpc.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/greeter_server.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto/greet.grpc.pb.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/py_grpc_hello_world_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_manager.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/utils/linux_headers.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::system::ProcPidPath;
using ::px::types::ColumnWrapperRecordBatch;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::StrEq;

StatusOr<md::UPID> ToUPID(uint32_t pid) {
  PX_ASSIGN_OR_RETURN(int64_t pid_start_time, system::GetPIDStartTimeTicks(ProcPidPath(pid)));
  return md::UPID{/* asid */ 0, pid, pid_start_time};
}

class GRPCServer {
 public:
  static constexpr std::string_view kServerPath =
      "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server/"
      "golang_$0_grpc_server";

  GRPCServer() = default;

  void LaunchServer(std::string go_version, bool use_https) {
    std::string server_path = absl::Substitute(kServerPath, go_version);
    server_path = px::testing::BazelRunfilePath(server_path).string();
    CHECK(fs::Exists(server_path));

    const std::string https_flag = use_https ? "--https=true" : "--https=false";
    // Let server pick random port in order avoid conflicting.
    const std::string port_flag = "--port=0";

    PX_CHECK_OK(s_.Start({server_path, https_flag, port_flag}));
    LOG(INFO) << "Server PID: " << s_.child_pid();

    // Give some time for the server to start up.
    sleep(2);

    std::string port_str;
    PX_CHECK_OK(s_.Stdout(&port_str));
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

  void LaunchClient(std::string_view go_version, bool use_compression, bool use_https, int port) {
    std::string client_path = absl::Substitute(kClientPath, go_version);
    client_path = px::testing::BazelRunfilePath(client_path).string();

    CHECK(fs::Exists(client_path));

    const std::string https_flag = use_https ? "--https=true" : "--https=false";
    const std::string compression_flag =
        use_compression ? "--compression=true" : "--compression=false";
    PX_CHECK_OK(c_.Start({client_path, https_flag, compression_flag, "-once", "-name=PixieLabs",
                          absl::StrCat("-address=localhost:", port)}));
    LOG(INFO) << "Client PID: " << c_.child_pid();
    CHECK_EQ(0, c_.Wait());
  }

  SubProcess c_;
};

struct TestParams {
  std::string go_version;
  bool use_compression;
  bool use_https;
};

using TestFixture = testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false>;

class GRPCTraceTest : public TestFixture, public ::testing::WithParamInterface<TestParams> {
 protected:
  GRPCTraceTest() {}

  void TearDown() override {
    TestFixture::TearDown();
    server_.s_.Kill();
    CHECK_EQ(9, server_.s_.Wait()) << "Server should have been killed.";
  }

  GRPCServer server_;
  GRPCClient client_;
};

TEST_P(GRPCTraceTest, CaptureRPCTraceRecord) {
  auto params = GetParam();

  PX_SET_FOR_SCOPE(FLAGS_socket_tracer_enable_http2_gzip, params.use_compression);
  server_.LaunchServer(params.go_version, params.use_https);

  // Deploy uprobes on the newly launched server.
  RefreshContext(/* blocking_deploy_uprobes */ true);

  StartTransferDataThread();

  client_.LaunchClient(params.go_version, params.use_compression, params.use_https, server_.port());

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& rb, tablets);
  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(rb, kHTTPUPIDIdx, server_.pid());
  ASSERT_GE(target_record_indices.size(), 1);

  // We should get exactly one record.
  const size_t idx = target_record_indices.front();
  const std::string scheme_text = params.use_https ? R"(":scheme":"https")" : R"(":scheme":"http")";

  md::UPID upid(rb[kHTTPUPIDIdx]->Get<types::UInt128Value>(idx).val);
  ASSERT_OK_AND_ASSIGN(md::UPID expected_upid, ToUPID(server_.pid()));
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
                         ::testing::Values(
                             // Did not enumerate all combinations, as they are independent based on
                             // our knowledge, and to minimize test size to reduce flakiness.
                             TestParams{"1_16", true, true}, TestParams{"1_16", true, false},
                             TestParams{"1_17", false, true}, TestParams{"1_17", false, false},
                             TestParams{"1_18", false, true}, TestParams{"1_18", true, false},
                             TestParams{"1_19", false, false}, TestParams{"1_19", true, true},
                             TestParams{"1_20", true, true}, TestParams{"1_20", true, false},
                             TestParams{"1_21", true, true}, TestParams{"1_21", true, false},
                             TestParams{"boringcrypto", true, true}));

class PyGRPCTraceTest : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
  PyGRPCTraceTest() {
    // This takes effect before initializing socket tracer, so Python gRPC is actually enabled.
    FLAGS_stirling_enable_grpc_c_tracing = true;

    // This enables uprobe attachment after stirling has started running.
    FLAGS_stirling_rescan_for_dlopen = true;
  }
};

// Test that socket tracker can trace Python gRPC server's message.
TEST_F(PyGRPCTraceTest, VerifyTraceRecords) {
  ASSERT_OK_AND_ASSIGN(const system::KernelVersion kernel_version, system::GetKernelVersion());
  const system::KernelVersion kKernelVersion5_3 = {5, 3, 0};
  if (system::CompareKernelVersions(kernel_version, kKernelVersion5_3) ==
      system::KernelVersionOrder::kOlder) {
    LOG(WARNING) << absl::Substitute(
        "Skipping because host kernel version $0 is older than $1, "
        "old kernel versions do not support bounded loops, which is required by grpc_c_trace.c",
        kernel_version.ToString(), kKernelVersion5_3.ToString());
    return;
  }

  StartTransferDataThread();

  testing::PyGRPCHelloWorld client;
  testing::PyGRPCHelloWorld server;

  // First start the server so the process can be detected by socket tracer.
  PX_CHECK_OK(server.Run(std::chrono::seconds{60}, /*options*/ {},
                         /*args*/ {"python", "helloworld/greeter_server.py"}));
  PX_CHECK_OK(
      client.Run(std::chrono::seconds{60},
                 /*options*/ {absl::Substitute("--network=container:$0", server.container_name())},
                 /*args*/ {"python", "helloworld/greeter_client.py"}));
  // The client sends only 1 request and then exits. So we can wait for it to finish.
  client.Wait();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& rb, tablets);

  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(rb, kHTTPUPIDIdx, server.process_pid());
  ASSERT_EQ(target_record_indices.size(), 1);
  const size_t idx = target_record_indices.front();

  md::UPID upid(rb[kHTTPUPIDIdx]->Get<types::UInt128Value>(idx).val);
  ASSERT_OK_AND_ASSIGN(md::UPID expected_upid, ToUPID(server.process_pid()));
  EXPECT_EQ(upid, expected_upid);

  EXPECT_THAT(
      rb[kHTTPReqHeadersIdx]->Get<types::StringValue>(idx).string(),
      AllOf(HasSubstr(R"(":authority":"localhost:50051")"), HasSubstr(R"(":method":"POST")"),
            HasSubstr(R"(":scheme":"http")"), HasSubstr(R"("content-type":"application/grpc")"),
            HasSubstr(R"("te":"trailers")")));
  EXPECT_THAT(rb[kHTTPReqBodyIdx]->Get<types::StringValue>(idx).string(), HasSubstr(R"(1: "you")"));
  EXPECT_THAT(rb[kHTTPRespBodyIdx]->Get<types::StringValue>(idx).string(),
              HasSubstr(R"(1: "Hello, you!")"));

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
}

}  // namespace stirling
}  // namespace px
