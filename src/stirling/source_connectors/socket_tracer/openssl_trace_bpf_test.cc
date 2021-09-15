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

using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTest;
using ::px::stirling::testing::ToRecordVector;

using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class NginxOpenSSL_1_1_0_ContainerWrapper
    : public ::px::stirling::testing::NginxOpenSSL_1_1_0_Container {
 public:
  int32_t PID() const { return NginxWorkerPID(); }
};

class NginxOpenSSL_1_1_1_ContainerWrapper
    : public ::px::stirling::testing::NginxOpenSSL_1_1_1_Container {
 public:
  int32_t PID() const { return NginxWorkerPID(); }
};

class NodeServerContainerWrapper : public ::px::stirling::testing::NodeServerContainer {
 public:
  int32_t PID() const { return process_pid(); }
};

template <typename ServerContainer>
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

  ServerContainer server_;
};

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

// The is the response to `GET /index.html`.
std::string_view kNginxRespBody = R"(<!DOCTYPE html>
<html>
<head>
<title>Welcome to nginx!</title>
<style>
    body {
        width: 35em;
        margin: 0 auto;
        font-family: Tahoma, Verdana, Arial, sans-serif;
    }
</style>
</head>
<body>
<h1>Welcome to nginx!</h1>
<p>If you see this page, the nginx web server is successfully installed and
working. Further configuration is required.</p>

<p>For online documentation and support please refer to
<a href="http://nginx.org/">nginx.org</a>.<br/>
Commercial support is available at
<a href... [TRUNCATED])";

http::Record GetExpectedHTTPRecord() {
  http::Record expected_record;
  expected_record.req.minor_version = 1;
  expected_record.req.req_method = "GET";
  expected_record.req.req_path = "/index.html";
  expected_record.req.body = "";
  expected_record.resp.resp_status = 200;
  expected_record.resp.resp_message = "OK";
  expected_record.resp.body = kNginxRespBody;
  return expected_record;
}

typedef ::testing::Types<NginxOpenSSL_1_1_0_ContainerWrapper, NginxOpenSSL_1_1_1_ContainerWrapper,
                         NodeServerContainerWrapper>
    OpenSSLServerImplementations;
TYPED_TEST_SUITE(OpenSSLTraceTest, OpenSSLServerImplementations);

TYPED_TEST(OpenSSLTraceTest, ssl_capture_curl_client) {
  this->StartTransferDataThread();

  // Make an SSL request with curl.
  // Because the server uses a self-signed certificate, curl will normally refuse to connect.
  // This is similar to the warning pages that Firefox/Chrome would display.
  // To take an exception and make the SSL connection anyways, we use the --insecure flag.

  // Run the client in the network of the server, so they can connect to each other.
  ::px::stirling::testing::CurlContainer client;
  PL_CHECK_OK(
      client.Run(std::chrono::seconds{60},
                 {absl::Substitute("--network=container:$0", this->server_.container_name())},
                 {"--insecure", "-s", "-S", "https://localhost:443/index.html"}));
  client.Wait();

  int server_pid = this->server_.PID();

  this->StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets =
      this->ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  for (size_t i = 0; i < record_batch[0]->Size(); ++i) {
    uint32_t pid = record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(i).High64();
    std::string req_path = record_batch[kHTTPReqPathIdx]->Get<types::StringValue>(i);
    VLOG(1) << absl::Substitute("$0 $1", pid, req_path);
  }

  http::Record expected_record = GetExpectedHTTPRecord();

  std::vector<size_t> server_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, server_pid);

  // Check server-side tracing results.
  {
    std::vector<http::Record> records = ToRecordVector(record_batch, server_record_indices);
    EXPECT_THAT(records, UnorderedElementsAre(EqHTTPRecord(expected_record)));
    EXPECT_THAT(testing::GetRemoteAddrs(record_batch, server_record_indices),
                UnorderedElementsAre(StrEq("127.0.0.1")));
  }
}

TYPED_TEST(OpenSSLTraceTest, ssl_capture_ruby_client) {
  this->StartTransferDataThread();

  // Make multiple requests and make sure we capture all of them.
  std::string rb_script = R"(
        require 'net/http'
        require 'uri'

        $i = 0
        while $i < 3 do
          uri = URI.parse('https://localhost:443/index.html')
          http = Net::HTTP.new(uri.host, uri.port)
          http.use_ssl = true
          http.verify_mode = OpenSSL::SSL::VERIFY_NONE
          request = Net::HTTP::Get.new(uri.request_uri)
          response = http.request(request)
          p response.body

          sleep(1)

          $i += 1
        end
  )";

  // Make an SSL request with the client.
  // Run the client in the network of the server, so they can connect to each other.
  ::px::stirling::testing::RubyContainer client;
  PL_CHECK_OK(
      client.Run(std::chrono::seconds{60},
                 {absl::Substitute("--network=container:$0", this->server_.container_name())},
                 {"ruby", "-e", rb_script}));
  client.Wait();

  int server_pid = this->server_.PID();

  this->StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets =
      this->ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  http::Record expected_record = GetExpectedHTTPRecord();

  std::vector<size_t> server_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, server_pid);

  // Check server-side tracing results.
  {
    std::vector<http::Record> records = ToRecordVector(record_batch, server_record_indices);
    EXPECT_THAT(records,
                UnorderedElementsAre(EqHTTPRecord(expected_record), EqHTTPRecord(expected_record),
                                     EqHTTPRecord(expected_record)));
    EXPECT_THAT(testing::GetRemoteAddrs(record_batch, server_record_indices),
                UnorderedElementsAre(StrEq("127.0.0.1"), StrEq("127.0.0.1"), StrEq("127.0.0.1")));
  }
}

TYPED_TEST(OpenSSLTraceTest, ssl_capture_node_client) {
  this->StartTransferDataThread();

  // Make an SSL request with the client.
  // Run the client in the network of the server, so they can connect to each other.
  ::px::stirling::testing::NodeClientContainer client;
  PL_CHECK_OK(
      client.Run(std::chrono::seconds{60},
                 {absl::Substitute("--network=container:$0", this->server_.container_name())},
                 {"node", "/etc/node/https_client.js"}));
  client.Wait();

  int server_pid = this->server_.PID();

  this->StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets =
      this->ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  http::Record expected_record = GetExpectedHTTPRecord();

  std::vector<size_t> server_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, server_pid);

  // Check server-side tracing results.
  {
    std::vector<http::Record> records = ToRecordVector(record_batch, server_record_indices);
    EXPECT_THAT(records, UnorderedElementsAre(EqHTTPRecord(expected_record)));
    EXPECT_THAT(testing::GetRemoteAddrs(record_batch, server_record_indices),
                UnorderedElementsAre(StrEq("127.0.0.1")));
  }
}

}  // namespace stirling
}  // namespace px
