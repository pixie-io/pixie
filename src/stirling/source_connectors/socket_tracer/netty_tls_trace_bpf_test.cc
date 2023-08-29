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

#include <string>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/thrift_mux_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace mux = protocols::mux;

using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::EqMuxRecord;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::px::stirling::testing::ToRecordVector;

using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class ThriftMuxServerContainerWrapper : public ::px::stirling::testing::ThriftMuxServerContainer {};

namespace {
constexpr bool kClientSideTracing = false;
}

template <typename TServerContainer>
class BaseOpenSSLTraceTest : public SocketTraceBPFTestFixture<kClientSideTracing> {
 protected:
  void SetUp() override {
    PX_SET_FOR_SCOPE(FLAGS_stirling_enable_mux_tracing, true);

    // We turn off CQL and NATS tracing to give some BPF instructions back for Mux.
    // This is required for older kernels with only 4096 BPF instructions.
    PX_SET_FOR_SCOPE(FLAGS_stirling_enable_cass_tracing, false);
    PX_SET_FOR_SCOPE(FLAGS_stirling_enable_nats_tracing, false);

    // Enable the raw fptr fallback for determining ssl lib version.
    PX_SET_FOR_SCOPE(FLAGS_openssl_raw_fptrs_enabled, true);

    SocketTraceBPFTestFixture<kClientSideTracing>::SetUp();
  }

  BaseOpenSSLTraceTest() {
    // Run the nginx HTTPS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTestFixture SetUp().
    StatusOr<std::string> run_result =
        server_.Run(std::chrono::seconds{60}, {}, {"--use-tls", "true"});
    PX_CHECK_OK(run_result);
    PX_CHECK_OK(this->RunThriftMuxClient());

    // Sleep an additional second, just to be safe.
    sleep(1);
  }

  StatusOr<int32_t> RunThriftMuxClient() {
    std::string classpath =
        "@/app/px/src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux/"
        "server_image.classpath";
    std::string thriftmux_client_output = "StringString";

    // The thriftmux container uses a CA created by
    // src/common/testing/test_utils/cert_generator:cert_generator.
    // This command adds this CA to java's keystore so ssl
    // verification works as expected.
    std::string keytool_cmd = absl::Substitute(
        "podman exec $0 /usr/lib/jvm/java-11-openjdk-amd64/bin/keytool -importcert -keystore "
        "/etc/ssl/certs/java/cacerts -file /etc/ssl/ca.crt -noprompt -storepass changeit",
        server_.container_name());
    px::Exec(keytool_cmd);

    // Runs the Client entrypoint inside the server container
    // capturing the Client process's pid for easier debugging.
    std::string cmd = absl::Substitute(
        "podman exec $0 /usr/bin/java -cp $1 Client --use-tls true & echo $$! && wait",
        server_.container_name(), classpath);
    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));

    LOG(INFO) << absl::Substitute("thriftmux client command output: '$0'", out);

    std::vector<std::string> tokens = absl::StrSplit(out, "\n");

    int32_t client_pid;
    if (!absl::SimpleAtoi(tokens[0], &client_pid)) {
      return error::Internal("Could not extract PID.");
    }

    LOG(INFO) << absl::Substitute("Client PID: $0", client_pid);

    if (!absl::StrContains(out, thriftmux_client_output)) {
      return error::Internal(
          "Expected output from thriftmux to include '$0', received '$1' instead",
          thriftmux_client_output, out);
    }
    return client_pid;
  }

  TServerContainer server_;
};

mux::Record RecordWithType(mux::Type req_type) {
  mux::Record r = {};
  r.req.type = static_cast<int8_t>(req_type);

  return r;
}

using NettyTLSServerImplementations = ::testing::Types<ThriftMuxServerContainerWrapper>;

template <typename T>
using NettyTLSTraceTest = BaseOpenSSLTraceTest<T>;

TYPED_TEST_SUITE(NettyTLSTraceTest, NettyTLSServerImplementations);

TYPED_TEST(NettyTLSTraceTest, mtls_thriftmux_client) {
  // Uncomment to enable tracing:
  // FLAGS_stirling_conn_trace_pid = this->server_.process_pid();
  this->StartTransferDataThread();

  ASSERT_OK(this->RunThriftMuxClient());

  this->StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = this->ConsumeRecords(SocketTraceConnector::kMuxTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  std::vector<mux::Record> server_records =
      GetTargetRecords<mux::Record>(record_batch, this->server_.process_pid());

  mux::Record tinitCheck = RecordWithType(mux::Type::kRerrOld);
  mux::Record tinit = RecordWithType(mux::Type::kTinit);
  mux::Record pingRecord = RecordWithType(mux::Type::kTping);
  mux::Record dispatchRecord = RecordWithType(mux::Type::kTdispatch);

  EXPECT_THAT(server_records, Contains(EqMuxRecord(tinitCheck)));
  EXPECT_THAT(server_records, Contains(EqMuxRecord(tinit)));
  EXPECT_THAT(server_records, Contains(EqMuxRecord(pingRecord)));
  EXPECT_THAT(server_records, Contains(EqMuxRecord(dispatchRecord)));
}

}  // namespace stirling
}  // namespace px
