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
#include "src/stirling/core/output.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/dns_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::FindRecordsMatchingPID;
using ::px::stirling::testing::RecordBatchSizeIs;
using ::px::stirling::testing::SocketTraceBPFTestFixture;

using ::testing::Each;
using ::testing::MatchesRegex;

class DNSTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ true> {
 protected:
  DNSTraceTest() {
    // Run the bind DNS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    PX_CHECK_OK(container_.Run(std::chrono::seconds{150}));
  }

  ::px::stirling::testing::DNSServerContainer container_;
};

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(DNSTraceTest, Capture) {
  StartTransferDataThread();

  // Uncomment to enable tracing:
  // FLAGS_stirling_conn_trace_pid = container_.process_pid();

  // Run dig to generate a DNS request.
  // Run it through bash, and return the PID, so we can use it to filter captured results.
  std::string cmd =
      absl::StrFormat("podman exec %s sh -c 'dig @127.0.0.1 server.dnstest.com & echo $! && wait'",
                      container_.container_name());
  ASSERT_OK_AND_ASSIGN(std::string out, px::Exec(cmd));
  LOG(INFO) << out;

  StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kDNSTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& rb, tablets);
  PX_LOG_VAR(PrintDNSTable(rb));

  // Check server-side.
  {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(tablets[0].records, kDNSUPIDIdx, container_.process_pid());

    ASSERT_THAT(records, RecordBatchSizeIs(1));

    const std::string& req_hdr = records[kDNSReqHdrIdx]->Get<types::StringValue>(0);
    const std::string& req_body = records[kDNSReqBodyIdx]->Get<types::StringValue>(0);
    const std::string& resp_hdr = records[kDNSRespHdrIdx]->Get<types::StringValue>(0);
    const std::string& resp_body = records[kDNSRespBodyIdx]->Get<types::StringValue>(0);

    EXPECT_THAT(
        req_hdr,
        MatchesRegex(
            R"(\{"txid":[0-9]+,"qr":0,"opcode":0,"aa":0,"tc":0,"rd":1,"ra":0,"ad":1,"cd":0,"rcode":0,)"
            R"("num_queries":1,"num_answers":0,"num_auth":0,"num_addl":1\})"));
    EXPECT_EQ(req_body, R"({"queries":[{"name":"server.dnstest.com","type":"A"}]})");
    EXPECT_THAT(
        resp_hdr,
        MatchesRegex(
            R"(\{"txid":[0-9]+,"qr":1,"opcode":0,"aa":1,"tc":0,"rd":1,"ra":1,"ad":0,"cd":0,"rcode":0,)"
            R"("num_queries":1,"num_answers":1,"num_auth":0,"num_addl":1\})"));
    EXPECT_EQ(resp_body,
              R"({"answers":[{"name":"server.dnstest.com","type":"A","addr":"192.168.32.200"}]})");
  }
}

}  // namespace stirling
}  // namespace px
