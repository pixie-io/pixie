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
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;
namespace mux = protocols::mux;

using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTest;
using ::px::stirling::testing::ToRecordVector;
using ::px::stirling::testing::ToMuxRecordVector;

using ::testing::Each;
using ::testing::Field;
using ::testing::MatchesRegex;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

using ::px::stirling::testing::ColWrapperSizeIs;
using ::testing::AllOf;
using ::testing::UnorderedElementsAreArray;


class ThriftmuxContainerWrapper
    : public ::px::stirling::testing::ThriftMuxServerContainer {
 public:
  int32_t PID() const { return process_pid(); }
};

// Includes all information we need to extract from the trace records, which are used to verify
// against the expected results.
struct TraceRecords {
  std::vector<http::Record> http_records;
  std::vector<std::string> remote_address;
};

struct MuxTraceRecords {
  std::vector<mux::Record> mux_records;
  std::vector<std::string> remote_address;
};

template <typename TServerContainer>
class OpenSSLTraceTest : public SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  OpenSSLTraceTest() {
    // Run the nginx HTTPS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    StatusOr<std::string> run_result = server_.Run(std::chrono::seconds{60});
    PL_CHECK_OK(run_result);

    // Sleep an additional second, just to be safe.
    sleep(1);
  }

  // Returns the trace records of the process specified by the input pid.
  MuxTraceRecords GetMuxTraceRecords(int pid) {
    std::vector<TaggedRecordBatch> tablets =
        this->ConsumeRecords(SocketTraceConnector::kMuxTableNum);
    if (tablets.empty()) {
      return {};
    }
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
    std::vector<size_t> server_record_indices =
        FindRecordIdxMatchesPID(record_batch, kMuxUPIDIdx, pid);
    std::vector<mux::Record> mux_records = ToMuxRecordVector(record_batch, server_record_indices);
    std::vector<std::string> remote_addresses =
        testing::GetRemoteAddrs(record_batch, server_record_indices);
    return {std::move(mux_records), std::move(remote_addresses)};
  }

  // Returns the trace records of the process specified by the input pid.
  TraceRecords GetTraceRecords(int pid) {
    std::vector<TaggedRecordBatch> tablets =
        this->ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
    if (tablets.empty()) {
      return {};
    }
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
    std::vector<size_t> server_record_indices =
        FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, pid);
    std::vector<http::Record> http_records = ToRecordVector(record_batch, server_record_indices);
    std::vector<std::string> remote_addresses =
        testing::GetRemoteAddrs(record_batch, server_record_indices);
    return {std::move(http_records), std::move(remote_addresses)};
  }

  TServerContainer server_;
};

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

typedef ::testing::Types<ThriftmuxContainerWrapper>
    OpenSSLServerImplementations;
TYPED_TEST_SUITE(OpenSSLTraceTest, OpenSSLServerImplementations);

inline auto EqMux(const mux::Frame& x) { return Field(&mux::Frame::type, ::testing::Eq(x.type)); }

inline auto EqMuxRecord(const mux::Record& x) {
  return AllOf(Field(&mux::Record::req, EqMux(x.req)), Field(&mux::Record::resp, EqMux(x.resp)));
}

mux::Record RecordWithType(mux::Type req_type) {
  mux::Record r = {};
  r.req.type = static_cast<int8_t>(req_type);

  return r;
}

StatusOr<int32_t> RunThriftMuxClient(std::string container_name) {

  std::string ca_setup_cmd = absl::StrFormat("docker exec %s /usr/lib/jvm/java-11-openjdk-amd64/bin/keytool -importcert -keystore /etc/ssl/certs/java/cacerts -file /etc/ssl/ca.crt -noprompt -storepass changeit", container_name);

  PL_ASSIGN_OR_RETURN(std::string out, px::Exec(ca_setup_cmd));
  LOG(INFO) << absl::StrFormat("thriftmux java keystore CA setup command output: '%s'", out);

  std::string classpath =
      "@/app/px/src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux/"
      "server_image.classpath";
  std::string cmd =
      absl::StrFormat("docker exec %s /usr/bin/java -cp %s Client & echo $! && wait", container_name, classpath);
  PL_ASSIGN_OR_RETURN(std::string out2, px::Exec(cmd));

  LOG(INFO) << absl::StrFormat("thriftmux client command output: '%s'", out2);

  std::vector<std::string> tokens = absl::StrSplit(out2, "\n");

  int32_t client_pid;
  if (!absl::SimpleAtoi(tokens[0], &client_pid)) {
    return error::Internal("Could not extract PID.");
  }

  LOG(INFO) << absl::StrFormat("Client PID: %d", client_pid);

  std::string thriftmux_client_output = "StringString";
  if (tokens[1] != thriftmux_client_output) {
    return error::Internal(
        absl::StrFormat("Expected output from thriftmux to be '%s', received '%s' instead",
                        thriftmux_client_output, tokens[1]));
  }
  return client_pid;
}

TYPED_TEST(OpenSSLTraceTest, mtls_thriftmux) {

  // The first run of the java client will cause /tmp/libnetty_tcnative_linux_x86.so
  // to be loaded. Not sure if striling can handle this, but let's make sure it's memory
  // mapped before we start the data thread transfer.
  RunThriftMuxClient(std::string(this->server_.container_name()));

  this->StartTransferDataThread();

  RunThriftMuxClient(std::string(this->server_.container_name()));

  this->StopTransferDataThread();

  MuxTraceRecords records = this->GetMuxTraceRecords(this->server_.PID());

  mux::Record tinitCheck = RecordWithType(mux::Type::kRerrOld);
  mux::Record tinit = RecordWithType(mux::Type::kTinit);
  mux::Record pingRecord = RecordWithType(mux::Type::kTping);
  mux::Record dispatchRecord = RecordWithType(mux::Type::kTdispatch);

  EXPECT_THAT(records.mux_records, Contains(EqMuxRecord(tinitCheck)));
  EXPECT_THAT(records.mux_records, Contains(EqMuxRecord(tinit)));
  EXPECT_THAT(records.mux_records, Contains(EqMuxRecord(pingRecord)));
  EXPECT_THAT(records.mux_records, Contains(EqMuxRecord(dispatchRecord)));
}


}  // namespace stirling
}  // namespace px
