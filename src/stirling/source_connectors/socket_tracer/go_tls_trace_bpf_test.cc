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

#include "src/common/base/test_utils.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;

// Automatically converts ToString() to stream operator for gtest.
using ::px::operator<<;

using ::px::stirling::testing::AccessRecordBatch;
using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::ToRecordVector;
using ::px::testing::BazelBinTestFilePath;

using ::testing::UnorderedElementsAre;

//-----------------------------------------------------------------------------
// Test Stimulus: Server and Client
//-----------------------------------------------------------------------------

class GoTLSServerContainer : public ContainerRunner {
 public:
  GoTLSServerContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_https/server/server_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "https_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTPS service";
};

class GoTLSClientContainer : public ContainerRunner {
 public:
  GoTLSClientContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_https/client/client_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "https_client";
  static constexpr std::string_view kReadyMessage = R"({"status":"ok"})";
};

//-----------------------------------------------------------------------------
// Test Class and Test Cases
//-----------------------------------------------------------------------------

class GoTLSTraceTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  GoTLSTraceTest() {
    // Run the server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    PL_CHECK_OK(server_.Run(std::chrono::seconds{60}, {}));
  }

  GoTLSServerContainer server_;
  GoTLSClientContainer client_;
};

//-----------------------------------------------------------------------------
// Result Checking: Helper Functions and Matchers
//-----------------------------------------------------------------------------

TEST_F(GoTLSTraceTest, Basic) {
  StartTransferDataThread();

  // Run the client in the network of the server, so they can connect to each other.
  PL_CHECK_OK(client_.Run(std::chrono::seconds{10},
                          {absl::Substitute("--network=container:$0", server_.container_name())}));
  client_.Wait();

  StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, server_.process_pid());

    std::vector<http::Record> records = ToRecordVector(record_batch, target_record_indices);

    // TODO(oazizi): Add headers checking too.
    http::Record expected_record = {};
    expected_record.req.req_path = "/";
    expected_record.req.req_method = "GET";
    expected_record.req.body = R"()";
    expected_record.resp.resp_status = 200;
    expected_record.resp.resp_message = "OK";
    expected_record.resp.body = R"({"status":"ok"})";

    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record)));
  }
}

}  // namespace stirling
}  // namespace px
