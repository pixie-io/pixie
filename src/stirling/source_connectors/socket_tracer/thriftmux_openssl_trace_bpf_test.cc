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
#include "src/stirling/core/data_table.h"
#include "src/common/testing/test_environment.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace mux = protocols::mux;

using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::EqMuxRecord;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTest;
using ::px::stirling::testing::ToRecordVector;

using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class ThriftMuxServerContainerWrapper : public ::px::stirling::testing::ThriftMuxServerContainer {
 public:
  int32_t PID() const { return process_pid(); }
};

// The Init() function is used to set flags for the entire test.
// We can't do this in the MuxTraceTest constructor, because it will be too late
// (SocketTraceBPFTest will already have been constructed).
bool Init() {
  // Make sure Mux tracing is enabled.
  FLAGS_stirling_enable_mux_tracing = true;

  // We turn off CQL tracing to give some BPF instructions back for Mux.
  // This is required for older kernels with only 4096 BPF instructions.
  FLAGS_stirling_enable_cass_tracing = false;
  return true;
}

bool kInit = Init();

template <typename TServerContainer, bool TForceFptrs>
class BaseOpenSSLTraceTest : public SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  BaseOpenSSLTraceTest() {
    // Run the nginx HTTPS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    StatusOr<std::string> run_result = server_.Run(std::chrono::seconds{60}, {}, {"--use-tls", "true"});
    PL_CHECK_OK(run_result);
    PL_CHECK_OK(this->RunThriftMuxClient());

    // Sleep an additional second, just to be safe.
    sleep(1);
  }

  void SetUp() override {
    FLAGS_openssl_force_raw_fptrs = force_fptr_;

    SocketTraceBPFTest::SetUp();
  }

  StatusOr<int32_t> RunThriftMuxClient() {

    std::string classpath =
      "@/app/px/src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux/"
      "server_image.classpath";
    std::string thriftmux_client_output = "StringString";

    std::string keytool_cmd =
        absl::StrFormat("docker exec %s /usr/lib/jvm/java-11-openjdk-amd64/bin/keytool -importcert -keystore /etc/ssl/certs/java/cacerts -file /etc/ssl/ca.crt -noprompt -storepass changeit",
                        server_.container_name());
    PL_ASSIGN_OR_RETURN(std::string keytool_out, px::Exec(keytool_cmd));

    LOG(INFO) << absl::StrFormat("keytool -importcert command output: '%s'", keytool_out);

    std::string cmd =
        absl::StrFormat("docker exec %s /usr/bin/java -cp %s Client --use-tls true & echo $! && wait",
                        server_.container_name(), classpath);
    PL_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));

    LOG(INFO) << absl::StrFormat("thriftmux client command output: '%s'", out);

    std::vector<std::string> tokens = absl::StrSplit(out, "\n");

    int32_t client_pid;
    if (!absl::SimpleAtoi(tokens[0], &client_pid)) {
      return error::Internal("Could not extract PID.");
    }

    LOG(INFO) << absl::StrFormat("Client PID: %d", client_pid);

    if (!absl::StrContains(out, thriftmux_client_output)) {
      return error::Internal(
          absl::StrFormat("Expected output from thriftmux to include '%s', received '%s' instead",
                          thriftmux_client_output, out));
    }
    return client_pid;
  }

  TServerContainer server_;
  bool force_fptr_ = TForceFptrs;
};

mux::Record RecordWithType(mux::Type req_type) {
  mux::Record r = {};
  r.req.type = static_cast<int8_t>(req_type);

  return r;
}

typedef ::testing::Types<ThriftMuxServerContainerWrapper>
    OpenSSLServerImplementations;

template <typename T>
using OpenSSLTraceDlsymTest = BaseOpenSSLTraceTest<T, false>;

template <typename T>
using OpenSSLTraceRawFptrsTest = BaseOpenSSLTraceTest<T, true>;

#define OPENSSL_TYPED_TEST(TestCase, CodeBlock)            \
  TYPED_TEST(OpenSSLTraceRawFptrsTest, TestCase)              \
  CodeBlock

TYPED_TEST_SUITE(OpenSSLTraceDlsymTest, OpenSSLServerImplementations);
TYPED_TEST_SUITE(OpenSSLTraceRawFptrsTest, OpenSSLServerImplementations);

OPENSSL_TYPED_TEST(mtls_thriftmux_client, {
  FLAGS_stirling_conn_trace_pid = this->server_.process_pid();
  this->StartTransferDataThread();

  ASSERT_OK(this->RunThriftMuxClient());

  this->StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = this->ConsumeRecords(SocketTraceConnector::kMuxTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  std::vector<mux::Record> server_records = GetTargetRecords<mux::Record>(record_batch, this->server_.process_pid());

  mux::Record tinitCheck = RecordWithType(mux::Type::kRerrOld);
  mux::Record tinit = RecordWithType(mux::Type::kTinit);
  mux::Record pingRecord = RecordWithType(mux::Type::kTping);
  mux::Record dispatchRecord = RecordWithType(mux::Type::kTdispatch);

  EXPECT_THAT(server_records, Contains(EqMuxRecord(tinitCheck)));
  EXPECT_THAT(server_records, Contains(EqMuxRecord(tinit)));
  EXPECT_THAT(server_records, Contains(EqMuxRecord(pingRecord)));
  EXPECT_THAT(server_records, Contains(EqMuxRecord(dispatchRecord)));
})

}  // namespace stirling
}  // namespace px
