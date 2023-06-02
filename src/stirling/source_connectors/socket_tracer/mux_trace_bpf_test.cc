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

#include <absl/strings/str_replace.h>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/output.h"
#include "src/stirling/source_connectors/socket_tracer/mux_table.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/thrift_mux_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/utils/linux_headers.h"

namespace px {
namespace stirling {

namespace mux = protocols::mux;

using ::px::stirling::testing::EqMuxRecord;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::FindRecordsMatchingPID;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::testing::AllOf;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

using ::testing::Each;
using ::testing::Field;
using ::testing::MatchesRegex;

// The Init() function is used to set flags for the entire test.
// We can't do this in the MuxTraceTest constructor, because it will be too late
// (SocketTraceBPFTest will already have been constructed).
bool Init() {
  // Make sure Mux tracing is enabled.
  FLAGS_stirling_enable_mux_tracing = true;

  // We turn off CQL and NATS tracing to give some BPF instructions back for Mux.
  // This is required for older kernels with only 4096 BPF instructions.
  FLAGS_stirling_enable_cass_tracing = false;
  FLAGS_stirling_enable_nats_tracing = false;
  return true;
}

class MuxTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ true> {
 protected:
  MuxTraceTest() {
    Init();

    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().

    // Run the thriftmux server and then the thriftmux client container
    PX_CHECK_OK(server_.Run(std::chrono::seconds{60}));
  }

  std::string thriftmux_client_output = "StringString";

  std::string classpath =
      "@/app/px/src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux/"
      "server_image.classpath";

  StatusOr<int32_t> RunThriftMuxClient() {
    std::string cmd =
        absl::StrFormat("podman exec %s /usr/bin/java -cp %s Client & echo $! && wait",
                        server_.container_name(), classpath);
    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));

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

  ::px::stirling::testing::ThriftMuxServerContainer server_;
};

std::vector<mux::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                        const std::vector<size_t>& indices) {
  std::vector<mux::Record> result;

  for (const auto& idx : indices) {
    mux::Record r;
    r.req.type = static_cast<int8_t>(rb[kMuxReqTypeIdx]->Get<types::Int64Value>(idx).val);
    result.push_back(r);
  }
  return result;
}

mux::Record RecordWithType(mux::Type req_type) {
  mux::Record r = {};
  r.req.type = static_cast<int8_t>(req_type);

  return r;
}

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(MuxTraceTest, Capture) {
  StartTransferDataThread();

  ASSERT_OK(RunThriftMuxClient());

  StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kMuxTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  std::vector<mux::Record> server_records =
      GetTargetRecords<mux::Record>(record_batch, server_.process_pid());

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
