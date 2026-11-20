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
#include <vector>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/curl_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nginx_openssl_3_0_8_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace tls = protocols::tls;

using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::px::stirling::testing::ToRecordVector;

using ::testing::HasSubstr;
using ::testing::IsTrue;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class NginxOpenSSL_3_0_8_ContainerWrapper
    : public ::px::stirling::testing::NginxOpenSSL_3_0_8_Container {
 public:
  int32_t PID() const { return NginxWorkerPID(); }
};

bool Init() {
  // Make sure TLS tracing is enabled.
  FLAGS_stirling_enable_tls_tracing = true;

  // We turn off CQL and NATS tracing to give some BPF instructions back for TLS.
  // This is required for older kernels with only 4096 BPF instructions.
  FLAGS_stirling_enable_cass_tracing = false;
  FLAGS_stirling_enable_nats_tracing = false;
  FLAGS_stirling_enable_amqp_tracing = false;
  return true;
}

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

tls::Record GetExpectedTLSRecord() {
  tls::Record expected_record;
  return expected_record;
}

class TLSVersionParameterizedTest
    : public SocketTraceBPFTestFixture</* TClientSideTracing */ false>,
      public ::testing::WithParamInterface<std::string> {
 protected:
  TLSVersionParameterizedTest() {
    Init();

    // Run the nginx HTTPS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    constexpr bool kHostPid = false;
    StatusOr<std::string> run_result = server_.Run(std::chrono::seconds{60}, {}, {}, kHostPid);
    PX_CHECK_OK(run_result);

    // Sleep an additional second, just to be safe.
    sleep(1);
  }

  void TestTLSVersion(const std::string& tls_version, const std::string& tls_max_version) {
    FLAGS_stirling_conn_trace_pid = this->server_.PID();

    this->StartTransferDataThread();

    // Make an SSL request with curl.
    ::px::stirling::testing::CurlContainer client;
    constexpr bool kHostPid = false;
    ASSERT_OK(
        client.Run(std::chrono::seconds{60},
                   {absl::Substitute("--network=container:$0", this->server_.container_name())},
                   {"--insecure", "-s", "-S", "--resolve", "test-host:443:127.0.0.1",
                    absl::Substitute("--tlsv$0", tls_version), "--tls-max", tls_max_version,
                    "https://test-host/index.html"},
                   kHostPid));
    client.Wait();
    this->StopTransferDataThread();

    auto records = this->GetTraceRecords(this->server_.PID());
    EXPECT_THAT(records, SizeIs(1));
    EXPECT_GT(records[0].req.body.size(), 0);
    EXPECT_THAT(records[0].req.body, HasSubstr(R"("server_name":["test-host"])"));
    EXPECT_THAT(records[0].req.body, HasSubstr(R"("alpn":[)"));
    EXPECT_THAT(records[0].resp.body, HasSubstr(R"("version":"TLSv1.2")"));
    EXPECT_THAT(records[0].resp.body, HasSubstr(R"("cipher_suite":"TLS_)"));

    std::vector<TaggedRecordBatch> http_tablets =
        this->ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
    ASSERT_FALSE(http_tablets.empty());
    types::ColumnWrapperRecordBatch http_batch = http_tablets[0].records;
    std::vector<size_t> http_indices =
        FindRecordIdxMatchesPID(http_batch, kHTTPUPIDIdx, this->server_.PID());
    ASSERT_FALSE(http_indices.empty());
    for (size_t idx : http_indices) {
      EXPECT_TRUE(http_batch[kHTTPEncryptedIdx]->Get<types::BoolValue>(idx).val);
      EXPECT_EQ(http_batch[kHTTPReqPathIdx]->Get<types::StringValue>(idx), "/index.html");
    }
  }

  // Returns the trace records of the process specified by the input pid.
  std::vector<tls::Record> GetTraceRecords(int pid) {
    std::vector<TaggedRecordBatch> tablets =
        this->ConsumeRecords(SocketTraceConnector::kTLSTableNum);
    if (tablets.empty()) {
      return {};
    }
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
    std::vector<size_t> server_record_indices =
        FindRecordIdxMatchesPID(record_batch, kTLSUPIDIdx, pid);
    return ToRecordVector<tls::Record>(record_batch, server_record_indices);
  }

  NginxOpenSSL_3_0_8_ContainerWrapper server_;
};

INSTANTIATE_TEST_SUITE_P(TLSVersions, TLSVersionParameterizedTest, ::testing::Values("1.2"));

TEST_P(TLSVersionParameterizedTest, TestTLSVersions) {
  const std::string& tls_version = GetParam();
  TestTLSVersion(tls_version, tls_version);
}

}  // namespace stirling
}  // namespace px
