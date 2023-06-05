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
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nginx_openssl_1_1_0_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/ruby_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;

// Automatically converts ToString() to stream operator for gtest.
using ::px::operator<<;

using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::GetTargetRecords;
using ::px::stirling::testing::SocketTraceBPFTestFixture;

using ::testing::UnorderedElementsAre;

using DynLibTraceTest = SocketTraceBPFTestFixture</* TClientSideTracing */ true>;

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

TEST_F(DynLibTraceTest, TraceDynLoadedOpenSSL) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_rescan_for_dlopen, true);
  PX_SET_FOR_SCOPE(FLAGS_stirling_rescan_exp_backoff_factor, 1.0);

  // Note that stirling is deployed before starting this test.

  // Makes the test run much faster.
  FLAGS_stirling_disable_self_tracing = true;

  StartTransferDataThread();

  ::px::stirling::testing::NginxOpenSSL_1_1_0_Container server;
  ::px::stirling::testing::RubyContainer client;

  // Run the nginx HTTPS server.
  // The container runner will make sure it is in the ready state before unblocking.
  ASSERT_OK_AND_ASSIGN(std::string run_result, server.Run(std::chrono::seconds{60}));

  // RefreshData will cause next TransferData to detect nginx, and deploy uprobes on its libssl.
  RefreshContext();

  {
    // The key to this test is that Ruby only loads OpenSSL when it's required,
    // which is when the line `http.verify_mode = OpenSSL::SSL::VERIFY_NONE` is executed.
    // Stirling will first detect the ruby binary without OpenSSL, but by sleeping
    // after OpenSSL is loaded, Stirling has a chance to deploy its uprobes,
    // and any subsequent requests are traced.
    //
    // Note: a previous version of this test did not have a sleep before the first reqeust,
    // and assumed that the first request would not be traced. However, because of a race between
    // Stirling and ruby, Stirling would still sometimes trace the first request, causing failure.
    // The current version is more robust to that race.
    std::string rb_script = R"(
          require 'net/http'
          require 'uri'

          # Let Stirling discover ruby when it has not yet loaded OpenSSL.
          sleep(1)

          uri = URI.parse('https://localhost:443/index.html')
          http = Net::HTTP.new(uri.host, uri.port)
          http.use_ssl = true
          http.verify_mode = OpenSSL::SSL::VERIFY_NONE # This line dynamically loads OpenSSL libs.

          # Give enough time for uprobes to load.
          sleep(5)

          # Make multiple requests, so we can check we trace them all.
          $i = 0
          while $i < 3 do
            request = Net::HTTP::Get.new(uri.request_uri)
            response = http.request(request)
            p response.body

            $i += 1
          end
)";

    // Make an SSL request with the client.
    // Run the client in the network of the server, so they can connect to each other.
    PX_CHECK_OK(client.Run(std::chrono::seconds{10},
                           {absl::Substitute("--network=container:$0", server.container_name())},
                           {"ruby", "-e", rb_script}));

    // Periodically run RefreshContext.
    // Do this at a frequency faster than the sleep in the Ruby script.
    // This is to detect libssl, and deploy uprobes.
    for (int i = 0; i < 20; ++i) {
      RefreshContext();
      sleep(1);
    }
    client.Wait();

    StopTransferDataThread();

    std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

    // Inspect records for Debug.
    for (size_t i = 0; i < record_batch[0]->Size(); ++i) {
      uint32_t pid = record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(i).High64();
      std::string req_path = record_batch[kHTTPReqPathIdx]->Get<types::StringValue>(i);
      std::string req_body = record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(i);
      std::string resp_body = record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(i);
      VLOG(1) << absl::Substitute("$0 req_path=$1 req_body=$2 resp_body=$3", pid, req_path,
                                  req_body, resp_body);
    }

    http::Record expected_record;
    expected_record.req.minor_version = 1;
    expected_record.req.req_method = "GET";
    expected_record.req.req_path = "/index.html";
    expected_record.req.body = "";
    expected_record.resp.resp_status = 200;
    expected_record.resp.resp_message = "OK";
    expected_record.resp.body = kNginxRespBody;

    // Check server-side tracing results.
    {
      // Nginx has a master process and a worker process. We need the PID of the worker process.
      int worker_pid;
      std::string pid_str =
          px::Exec(absl::Substitute("pgrep -P $0", server.process_pid())).ValueOrDie();
      ASSERT_TRUE(absl::SimpleAtoi(pid_str, &worker_pid));
      LOG(INFO) << absl::Substitute("Worker thread PID: $0", worker_pid);

      std::vector<http::Record> records = GetTargetRecords<http::Record>(record_batch, worker_pid);

      EXPECT_THAT(records,
                  UnorderedElementsAre(EqHTTPRecord(expected_record), EqHTTPRecord(expected_record),
                                       EqHTTPRecord(expected_record)));
    }

    // Check client-side tracing results.
    {
      std::vector<http::Record> records =
          GetTargetRecords<http::Record>(record_batch, client.process_pid());

      EXPECT_THAT(records,
                  UnorderedElementsAre(EqHTTPRecord(expected_record), EqHTTPRecord(expected_record),
                                       EqHTTPRecord(expected_record)));
    }
  }
}

}  // namespace stirling
}  // namespace px
