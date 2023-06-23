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
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/bssl_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/curl_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;

using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::px::stirling::testing::ToRecordVector;

class Bssl_ContainerWrapper : public ::px::stirling::testing::BsslContainer {};

// Includes all information we need to extract from the trace records, which are used to verify
// against the expected results.
struct TraceRecords {
  std::vector<http::Record> http_records;
  std::vector<std::string> remote_address;
};

template <typename TServerContainer>
class BoringSSLTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
  BoringSSLTraceTest() {
    // Run the nginx HTTPS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    StatusOr<std::string> run_result = server_.Run(std::chrono::seconds{60});
    PX_CHECK_OK(run_result);

    // Sleep an additional second, just to be safe.
    sleep(1);
  }

  void SetUp() override {
    FLAGS_stirling_trace_static_tls_binaries = true;

    SocketTraceBPFTestFixture::SetUp();
  }

  // Returns the trace records of the process specified by the input pid.
  TraceRecords GetTraceRecords(int pid) {
    std::vector<TaggedRecordBatch> tablets =
        this->ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
    if (tablets.empty()) {
      LOG(WARNING) << "GetTraceRecords is returning empty!";
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

typedef ::testing::Types<Bssl_ContainerWrapper> BoringSSLServerImplementations;

TYPED_TEST_SUITE(BoringSSLTraceTest, BoringSSLServerImplementations);

TYPED_TEST(BoringSSLTraceTest, ssl_capture_curl_client) {
  this->StartTransferDataThread();

  // Make an SSL request with curl.
  // Because the server uses a self-signed certificate, curl will normally refuse to connect.
  // This is similar to the warning pages that Firefox/Chrome would display.
  // To take an exception and make the SSL connection anyways, we use the --insecure flag.

  // Run the client in the network of the server, so they can connect to each other.
  ::px::stirling::testing::CurlContainer client;
  ASSERT_OK(
      client.Run(std::chrono::seconds{60},
                 {absl::Substitute("--network=container:$0", this->server_.container_name())},
                 {"--user-agent", "Testing", "--insecure", "-s", "-S", "https://localhost:4433/"}));
  client.Wait();
  this->StopTransferDataThread();

  TraceRecords records = this->GetTraceRecords(this->server_.process_pid());

  EXPECT_EQ(records.http_records.size(), 1);
  EXPECT_EQ(records.http_records[0].req.req_path, "/");
}

}  // namespace stirling
}  // namespace px
