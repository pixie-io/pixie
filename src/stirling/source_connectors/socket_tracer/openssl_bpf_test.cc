#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {

namespace http = protocols::http;

using ::pl::testing::BazelBinTestFilePath;
using ::pl::testing::TestFilePath;

using ::pl::stirling::testing::EqHTTPRecord;
using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::stirling::testing::GetTargetRecords;
using ::pl::stirling::testing::SocketTraceBPFTest;
using ::pl::stirling::testing::ToRecordVector;

using ::testing::UnorderedElementsAre;

class NginxOpenSSL_1_1_0_Container : public ContainerRunner {
 public:
  NginxOpenSSL_1_1_0_Container()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  // Image is a modified nginx image created through bazel rules, and stored as a tar file.
  // It is not pushed to any repo.
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/ssl/nginx_openssl_1_1_0_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "nginx";
  static constexpr std::string_view kReadyMessage = "";
};

class NginxOpenSSL_1_1_1_Container : public ContainerRunner {
 public:
  NginxOpenSSL_1_1_1_Container()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  // Image is a modified nginx image created through bazel rules, and stored as a tar file.
  // It is not pushed to any repo.
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/ssl/nginx_openssl_1_1_1_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "nginx";
  static constexpr std::string_view kReadyMessage = "";
};

class CurlContainer : public ContainerRunner {
 public:
  CurlContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix, kReadyMessage) {
  }

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/curl_container_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "curl";
  static constexpr std::string_view kReadyMessage = "";
};

class RubyContainer : public ContainerRunner {
 public:
  RubyContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix, kReadyMessage) {
  }

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/ruby_container_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "ruby";
  static constexpr std::string_view kReadyMessage = "";
};

template <typename NginxContainer>
class OpenSSLTraceTest : public SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  OpenSSLTraceTest() {
    // Run the nginx HTTPS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    // Note that this step will make an access to docker hub to download the container image.
    StatusOr<std::string> run_result = server_.Run(60);
    PL_CHECK_OK(run_result);

    // Sleep an additional second, just to be safe.
    sleep(1);
  }

  int NginxWorkerPID() {
    // Nginx has a master process and a worker process. We need the PID of the worker process.
    int worker_pid;
    std::string pid_str =
        pl::Exec(absl::Substitute("pgrep -P $0", this->server_.process_pid())).ValueOrDie();
    CHECK(absl::SimpleAtoi(pid_str, &worker_pid));
    LOG(INFO) << absl::Substitute("Worker thread PID: $0", worker_pid);
    return worker_pid;
  }

  NginxContainer server_;
};

using OpenSSL_1_1_0_TraceTest = OpenSSLTraceTest<NginxOpenSSL_1_1_0_Container>;
using OpenSSL_1_1_1_TraceTest = OpenSSLTraceTest<NginxOpenSSL_1_1_1_Container>;

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

typedef ::testing::Types<NginxOpenSSL_1_1_0_Container, NginxOpenSSL_1_1_1_Container>
    NginxImplementations;
TYPED_TEST_SUITE(OpenSSLTraceTest, NginxImplementations);

TYPED_TEST(OpenSSLTraceTest, ssl_capture_curl_client) {
  this->StartTransferDataThread(SocketTraceConnector::kHTTPTableNum, kHTTPTable);

  // Make an SSL request with curl.
  // Because the server uses a self-signed certificate, curl will normally refuse to connect.
  // This is similar to the warning pages that Firefox/Chrome would display.
  // To take an exception and make the SSL connection anyways, we use the --insecure flag.

  // Run the client in the network of the server, so they can connect to each other.
  CurlContainer client;
  PL_CHECK_OK(
      client.Run(10, {absl::Substitute("--network=container:$0", this->server_.container_name())},
                 {"--insecure", "-s", "-S", "https://localhost:443/index.html"}));
  client.Wait();

  int worker_pid = this->NginxWorkerPID();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = this->StopTransferDataThread();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  for (size_t i = 0; i < record_batch[0]->Size(); ++i) {
    uint32_t pid = record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(i).High64();
    std::string req_path = record_batch[kHTTPReqPathIdx]->Get<types::StringValue>(i);
    VLOG(1) << absl::Substitute("$0 $1", pid, req_path);
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
    std::vector<http::Record> records = GetTargetRecords(record_batch, worker_pid);

    EXPECT_THAT(records, UnorderedElementsAre(EqHTTPRecord(expected_record)));
  }
}

TYPED_TEST(OpenSSLTraceTest, ssl_capture_ruby_client) {
  this->StartTransferDataThread(SocketTraceConnector::kHTTPTableNum, kHTTPTable);

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
  RubyContainer client;
  PL_CHECK_OK(
      client.Run(10, {absl::Substitute("--network=container:$0", this->server_.container_name())},
                 {"ruby", "-e", rb_script}));
  client.Wait();

  int worker_pid = this->NginxWorkerPID();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = this->StopTransferDataThread();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

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
    std::vector<http::Record> records = GetTargetRecords(record_batch, worker_pid);

    EXPECT_THAT(records,
                UnorderedElementsAre(EqHTTPRecord(expected_record), EqHTTPRecord(expected_record),
                                     EqHTTPRecord(expected_record)));
  }
}

}  // namespace stirling
}  // namespace pl
