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

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_17_tls_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_17_tls_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_18_tls_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_18_tls_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_19_tls_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_19_tls_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_20_tls_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_20_tls_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_21_tls_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_21_tls_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_boringcrypto_tls_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_boringcrypto_tls_server_container.h"
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

using ::testing::UnorderedElementsAre;

//-----------------------------------------------------------------------------
// Test Class and Test Cases
//-----------------------------------------------------------------------------

template <typename TClientServerContainers>
class GoTLSTraceTest : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
  GoTLSTraceTest() {
    // Run the server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    PX_CHECK_OK(server_.Run(std::chrono::seconds{60}, {}));
  }

  typename TClientServerContainers::GoTLSServerContainer server_;
  typename TClientServerContainers::GoTLSClientContainer client_;
};

struct Go1_17TLSClientServerContainers {
  using GoTLSServerContainer = ::px::stirling::testing::Go1_17_TLSServerContainer;
  using GoTLSClientContainer = ::px::stirling::testing::Go1_17_TLSClientContainer;
};

struct Go1_18TLSClientServerContainers {
  using GoTLSServerContainer = ::px::stirling::testing::Go1_18_TLSServerContainer;
  using GoTLSClientContainer = ::px::stirling::testing::Go1_18_TLSClientContainer;
};

struct Go1_19TLSClientServerContainers {
  using GoTLSServerContainer = ::px::stirling::testing::Go1_19_TLSServerContainer;
  using GoTLSClientContainer = ::px::stirling::testing::Go1_19_TLSClientContainer;
};

struct Go1_20TLSClientServerContainers {
  using GoTLSServerContainer = ::px::stirling::testing::Go1_20_TLSServerContainer;
  using GoTLSClientContainer = ::px::stirling::testing::Go1_20_TLSClientContainer;
};

struct Go1_21TLSClientServerContainers {
  using GoTLSServerContainer = ::px::stirling::testing::Go1_21_TLSServerContainer;
  using GoTLSClientContainer = ::px::stirling::testing::Go1_21_TLSClientContainer;
};

struct GoBoringCryptoTLSClientServerContainers {
  using GoTLSServerContainer = ::px::stirling::testing::GoBoringCryptoTLSServerContainer;
  using GoTLSClientContainer = ::px::stirling::testing::GoBoringCryptoTLSClientContainer;
};

typedef ::testing::Types<Go1_17TLSClientServerContainers, Go1_18TLSClientServerContainers,
                         Go1_19TLSClientServerContainers, Go1_20TLSClientServerContainers,
                         Go1_21TLSClientServerContainers, GoBoringCryptoTLSClientServerContainers>
    GoVersions;
TYPED_TEST_SUITE(GoTLSTraceTest, GoVersions);

//-----------------------------------------------------------------------------
// Result Checking: Helper Functions and Matchers
//-----------------------------------------------------------------------------

TYPED_TEST(GoTLSTraceTest, BasicHTTP) {
  this->StartTransferDataThread();

  // Run the client in the network of the server, so they can connect to each other.
  PX_CHECK_OK(this->client_.Run(
      std::chrono::seconds{10},
      {absl::Substitute("--network=container:$0", this->server_.container_name())},
      {"--http2=false", "--iters=2", "--sub_iters=5"}));
  this->client_.Wait();

  this->StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets =
      this->ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, this->server_.process_pid());

    std::vector<http::Record> records =
        ToRecordVector<http::Record>(record_batch, target_record_indices);

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

TYPED_TEST(GoTLSTraceTest, BasicHTTP2) {
  this->StartTransferDataThread();

  // Run the client in the network of the server, so they can connect to each other.
  PX_CHECK_OK(this->client_.Run(
      std::chrono::seconds{10},
      {absl::Substitute("--network=container:$0", this->server_.container_name())},
      {"--http2=true", "--iters=2", "--sub_iters=5"}));
  this->client_.Wait();

  this->StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets =
      this->ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, this->server_.process_pid());

    std::vector<http::Record> records =
        ToRecordVector<http::Record>(record_batch, target_record_indices);

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
