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
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/output.h"
#include "src/stirling/source_connectors/socket_tracer/mux_table.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace mux = protocols::mux;

using ::px::stirling::testing::ColWrapperSizeIs;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::FindRecordsMatchingPID;
using ::px::stirling::testing::SocketTraceBPFTest;
using ::testing::AllOf;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

using ::testing::Each;
using ::testing::Field;
using ::testing::MatchesRegex;

class MuxTraceTest : public SocketTraceBPFTest</* TClientSideTracing */ true> {
 protected:
  MuxTraceTest() {
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().

    // Run the thriftmux server and then the thriftmux client container
    PL_CHECK_OK(server_.Run(std::chrono::seconds{60}));
  }

  std::string thriftmux_client_output = "StringString";

  StatusOr<int32_t> RunThriftMuxClient() {
    std::string cmd = absl::StrFormat("docker exec %s bash -c 'client & echo $! && wait'",
                                      server_.container_name());
    PL_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));

    LOG(INFO) << absl::StrFormat("thriftmux client command output: '%s'", out);

    std::vector<std::string> tokens = absl::StrSplit(out, "\n");

    int32_t client_pid;
    if (!absl::SimpleAtoi(tokens[0], &client_pid)) {
      return error::Internal("Could not extract PID.");
    }

    LOG(INFO) << absl::StrFormat("Client PID: %d", client_pid);

    if (tokens[1] != thriftmux_client_output) {
      return error::Internal(
          absl::StrFormat("Expected output from thriftmux to be '%s', received '%s' instead",
                          thriftmux_client_output, tokens[1]));
    }
    return client_pid;
  }

  ::px::stirling::testing::ThriftMuxContainer server_;
};

std::vector<mux::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                        const std::vector<size_t>& indices) {
  std::vector<mux::Record> result;

  for (const auto& idx : indices) {
    mux::Record r;
    uint24_t tag = static_cast<uint24_t>(rb[kMuxTagIdx]->Get<types::Int64Value>(idx).val);
    r.req.tag = tag;
    r.req.type = static_cast<int8_t>(rb[kMuxReqTypeIdx]->Get<types::Int64Value>(idx).val);
    r.resp.tag = tag;
    r.resp.type = static_cast<int8_t>(rb[kMuxRespTypeIdx]->Get<types::Int64Value>(idx).val);
    result.push_back(r);
  }
  return result;
}

std::vector<mux::Record> GetTargetRecords(const types::ColumnWrapperRecordBatch& record_batch,
                                          int32_t pid) {
  std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kMuxUPIDIdx, pid);
  return ToRecordVector(record_batch, target_record_indices);
}

inline auto EqMux(const mux::Frame& x) {
  return AllOf(Field(&mux::Frame::type, ::testing::Eq(x.type)),
               Field(&mux::Frame::tag, ::testing::Eq(x.tag)));
}

inline auto EqMuxRecord(const mux::Record& x) {
  return AllOf(Field(&mux::Record::req, EqMux(x.req)), Field(&mux::Record::resp, EqMux(x.resp)));
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
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  std::vector<mux::Record> server_records = GetTargetRecords(record_batch, server_.process_pid());

  mux::Record expected = {};
  expected.req.tag = 1;
  expected.req.type = 127;
  expected.resp.tag = 1;
  expected.resp.type = 127;

  // TODO(ddelnano): Figure out why only the Rerr message is recorded.
  // There should be a Tinit/Rinit, Tdispatch/Rdispatch and Tping/Rping as well
  EXPECT_THAT(server_records, Contains(EqMuxRecord(expected)));
}

}  // namespace stirling
}  // namespace px
