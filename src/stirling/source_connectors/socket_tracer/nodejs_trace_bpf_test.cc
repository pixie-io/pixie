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

#include <prometheus/text_serializer.h>
#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/curl_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/acorn_node14_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;

using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::EqHTTPRecordWithBodyRegex;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::px::stirling::testing::ToRecordVector;

using ::testing::StrEq;
using ::testing::Types;
using ::testing::UnorderedElementsAre;

class AcornNode14ContainerWrapper : public ::px::stirling::testing::AcornNode14Container {
 public:
  int32_t PID() const { return process_pid(); }
};

// Includes all information we need to extract from the trace records, which are used to verify
// against the expected results.
struct TraceRecords {
  std::vector<http::Record> http_records;
  std::vector<std::string> remote_address;
};

template <typename TServerContainer>
class NodeJSTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
  NodeJSTraceTest() {
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    StatusOr<std::string> run_result = server_.Run(std::chrono::seconds{60});
    PX_CHECK_OK(run_result);

    // Sleep an additional second, just to be safe.
    sleep(1);
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
    std::vector<http::Record> http_records =
        ToRecordVector<http::Record>(record_batch, server_record_indices);
    std::vector<std::string> remote_addresses =
        testing::GetRemoteAddrs(record_batch, server_record_indices);
    return {std::move(http_records), std::move(remote_addresses)};
  }

  TServerContainer server_;
};

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

std::string_view kPartialRespBody = R"delimiter(
295885c4 200 4096 8eee8a560fc1b68e046e9cb5de9a4c64b146667c93280582f5105914c333144

8eee8a560fc1b68e046e9cb5de9a4c64b146667c93280582f5105914c333144
8eee8a560fc1b68e046e9cb5de9a4c64b146667c93280582f5105914c333144
8eee8a560fc1b68e046e9cb5de9a4c64b146667c93280582f5105914c333144
8eee8a560fc1b68e046e9cb5de9a4c64b146667c93280582f5105914c333144
8eee8a560fc1b68e046e9cb5de9a4c64b146667c93280582f5105914c333144
8eee8a560fc1b68e046e9cb5de9a4c64b146667c93280582f5105914c333144
8eee8a560fc1b68e046e9cb5de9a4c64b146667c93280... [TRUNCATED])delimiter";

http::Record GetExpectedHTTPRecord() {
  http::Record expected_record;
  expected_record.req.req_method = "GET";
  expected_record.req.req_path = "/";
  expected_record.req.body = "";

  expected_record.resp.resp_status = 200;
  expected_record.resp.resp_message = "OK";
  return expected_record;
}

using NodeJSServerImplementations = Types<AcornNode14ContainerWrapper>;

TYPED_TEST_SUITE(NodeJSTraceTest, NodeJSServerImplementations);

TYPED_TEST(NodeJSTraceTest, simple_curl_client) {
  FLAGS_stirling_conn_trace_pid = this->server_.process_pid();
  this->StartTransferDataThread();

  // Run the client in the network of the server, so they can connect to each other.
  ::px::stirling::testing::CurlContainer client;
  ASSERT_OK(client.Run(std::chrono::seconds{60},
                       {absl::Substitute("--network=container:$0", this->server_.container_name())},
                       {"-s", "-S", "localhost:8080"}));
  client.Wait();
  this->StopTransferDataThread();

  TraceRecords records = this->GetTraceRecords(this->server_.PID());

  http::Record expected_record = GetExpectedHTTPRecord();

  if (LazyParsingEnabled()) {
    // We lazily parse the incomplete chunk with a partial body.
    // only the consistent part of the response body is checked
    EXPECT_THAT(records.http_records,
                UnorderedElementsAre(EqHTTPRecordWithBodyRegex(expected_record, ".*200 4096.*")));

    EXPECT_THAT(records.remote_address, UnorderedElementsAre(StrEq("127.0.0.1")));
  } else {
    // No records should be captured because we exceeded the loop limit (iovec has length 257) 
    // and lazy parsing was disabled.
    EXPECT_THAT(records.http_records, UnorderedElementsAre());
  }

  auto& registry = GetMetricsRegistry();  // retrieves global var keeping metrics
  auto metrics = registry.Collect();      // samples all metrics
  auto metrics_text = prometheus::TextSerializer().Serialize(metrics);  // serializes to text
  LOG(WARNING) << absl::Substitute("with metric text: $0", metrics_text);
}

}  // namespace stirling
}  // namespace px
