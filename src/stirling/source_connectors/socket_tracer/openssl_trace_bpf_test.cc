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
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/curl_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nginx_openssl_1_1_0_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nginx_openssl_1_1_1_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nginx_openssl_3_0_8_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/node_12_3_1_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/node_14_18_1_alpine_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/node_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/python_3_10_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/ruby_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;

using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::px::stirling::testing::ToRecordVector;

using ::testing::StrEq;
using ::testing::Types;
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

class NginxOpenSSL_3_0_8_ContainerWrapper
    : public ::px::stirling::testing::NginxOpenSSL_3_0_8_Container {
 public:
  int32_t PID() const { return NginxWorkerPID(); }
};

class Node12_3_1ContainerWrapper : public ::px::stirling::testing::Node12_3_1Container {
 public:
  int32_t PID() const { return process_pid(); }
};

class Node14_18_1AlpineContainerWrapper
    : public ::px::stirling::testing::Node14_18_1AlpineContainer {
 public:
  int32_t PID() const { return process_pid(); }
};

class Python310ContainerWrapper : public ::px::stirling::testing::Python310Container {
 public:
  int32_t PID() const { return process_pid(); }
};

// Includes all information we need to extract from the trace records, which are used to verify
// against the expected results.
struct TraceRecords {
  std::vector<http::Record> http_records;
  std::vector<std::string> remote_address;
  std::vector<std::string> local_address;
};

template <typename TServerContainer>
class OpenSSLTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
  OpenSSLTraceTest() {
    // Run the nginx HTTPS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    constexpr bool kHostPid = false;
    StatusOr<std::string> run_result = server_.Run(std::chrono::seconds{60}, {}, {}, kHostPid);
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
    std::vector<std::string> local_address =
        testing::GetLocalAddrs(record_batch, server_record_indices);
    return {std::move(http_records), std::move(remote_addresses), std::move(local_address)};
  }

  TServerContainer server_;
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

using OpenSSLServerImplementations =
    Types<NginxOpenSSL_1_1_0_ContainerWrapper, NginxOpenSSL_1_1_1_ContainerWrapper,
          NginxOpenSSL_3_0_8_ContainerWrapper, Python310ContainerWrapper,
          Node12_3_1ContainerWrapper, Node14_18_1AlpineContainerWrapper>;

TYPED_TEST_SUITE(OpenSSLTraceTest, OpenSSLServerImplementations);

TYPED_TEST(OpenSSLTraceTest, ssl_capture_curl_client) {
  this->StartTransferDataThread();

  // Make an SSL request with curl.
  // Because the server uses a self-signed certificate, curl will normally refuse to connect.
  // This is similar to the warning pages that Firefox/Chrome would display.
  // To take an exception and make the SSL connection anyways, we use the --insecure flag.

  // Run the client in the network of the server, so they can connect to each other.
  ::px::stirling::testing::CurlContainer client;
  constexpr bool kHostPid = false;
  ASSERT_OK(client.Run(std::chrono::seconds{60},
                       {absl::Substitute("--network=container:$0", this->server_.container_name())},
                       {"--insecure", "-s", "-S", "https://127.0.0.1:443/index.html"}, kHostPid));
  client.Wait();
  this->StopTransferDataThread();

  TraceRecords records = this->GetTraceRecords(this->server_.PID());
  http::Record expected_record = GetExpectedHTTPRecord();

  EXPECT_THAT(records.http_records, UnorderedElementsAre(EqHTTPRecord(expected_record)));
  EXPECT_THAT(records.remote_address, UnorderedElementsAre(StrEq("127.0.0.1")));
  // Due to loopback, the local address is the same as the remote address.
  EXPECT_THAT(records.local_address, UnorderedElementsAre(StrEq("127.0.0.1")));
}

TYPED_TEST(OpenSSLTraceTest, ssl_capture_ruby_client) {
  this->StartTransferDataThread();

  // Make multiple requests and make sure we capture all of them.
  std::string rb_script = R"(
        require 'net/http'
        require 'uri'

        $i = 0
        while $i < 3 do
          uri = URI.parse('https://127.0.0.1:443/index.html')
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
  constexpr bool kHostPid = false;
  ASSERT_OK(client.Run(std::chrono::seconds{60},
                       {absl::Substitute("--network=container:$0", this->server_.container_name())},
                       {"ruby", "-e", rb_script}, kHostPid));
  client.Wait();
  this->StopTransferDataThread();

  TraceRecords records = this->GetTraceRecords(this->server_.PID());
  http::Record expected_record = GetExpectedHTTPRecord();

  EXPECT_THAT(records.http_records,
              UnorderedElementsAre(EqHTTPRecord(expected_record), EqHTTPRecord(expected_record),
                                   EqHTTPRecord(expected_record)));
  EXPECT_THAT(records.remote_address,
              UnorderedElementsAre(StrEq("127.0.0.1"), StrEq("127.0.0.1"), StrEq("127.0.0.1")));
}

TYPED_TEST(OpenSSLTraceTest, ssl_capture_node_client) {
  this->StartTransferDataThread();

  // Make an SSL request with the client.
  // Run the client in the network of the server, so they can connect to each other.
  ::px::stirling::testing::NodeClientContainer client;
  constexpr bool kHostPid = false;
  ASSERT_OK(client.Run(std::chrono::seconds{60},
                       {absl::Substitute("--network=container:$0", this->server_.container_name())},
                       {"node", "/etc/node/https_client.js"}, kHostPid));
  client.Wait();
  this->StopTransferDataThread();

  TraceRecords records = this->GetTraceRecords(this->server_.PID());
  http::Record expected_record = GetExpectedHTTPRecord();

  EXPECT_THAT(records.http_records, UnorderedElementsAre(EqHTTPRecord(expected_record)));
  EXPECT_THAT(records.remote_address, UnorderedElementsAre(StrEq("127.0.0.1")));
}

}  // namespace stirling
}  // namespace px
