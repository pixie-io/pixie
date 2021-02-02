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
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {

namespace http = protocols::http;

using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::stirling::testing::SocketTraceBPFTest;
using ::pl::testing::BazelBinTestFilePath;
using ::pl::testing::TestFilePath;
using ::pl::types::ColumnWrapper;
using ::pl::types::ColumnWrapperRecordBatch;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class NginxContainer : public ContainerRunner {
 public:
  NginxContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  // Image is a modified nginx image created through bazel rules, and stored as a tar file.
  // It is not pushed to any repo.
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/ssl/nginx_openssl_1_1_0_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "nginx";
  static constexpr std::string_view kReadyMessage = "";
};

class RubyContainer : public ContainerRunner {
 public:
  RubyContainer() : ContainerRunner(kImageName, kContainerNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImageName = "ruby:3.0.0-buster";
  static constexpr std::string_view kContainerNamePrefix = "ruby";
  static constexpr std::string_view kReadyMessage = "";
};

using DynLibTraceTest = SocketTraceBPFTest</* TClientSideTracing */ true>;

//-----------------------------------------------------------------------------
// Utility Functions and Matchers
//-----------------------------------------------------------------------------

std::vector<http::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                         const std::vector<size_t>& indices) {
  std::vector<http::Record> result;

  for (const auto& idx : indices) {
    http::Record r;
    r.req.req_path = rb[kHTTPReqPathIdx]->Get<types::StringValue>(idx);
    r.req.minor_version = rb[kHTTPMinorVersionIdx]->Get<types::Int64Value>(idx).val;
    r.resp.resp_status = rb[kHTTPRespStatusIdx]->Get<types::Int64Value>(idx).val;
    r.req.resp_message = rb[kHTTPRespMessageIdx]->Get<types::StringValue>(idx);
    result.push_back(r);
  }
  return result;
}

auto EqHTTPReq(const http::Message& x) {
  return AllOf(Field(&http::Message::req_path, Eq(x.req_path)),
               Field(&http::Message::minor_version, Eq(x.minor_version)));
}

auto EqHTTPResp(const http::Message& x) {
  return AllOf(Field(&http::Message::resp_status, Eq(x.resp_status)));
}

auto EqHTTPRecord(const http::Record& x) {
  return AllOf(Field(&http::Record::req, EqHTTPReq(x.req)),
               Field(&http::Record::resp, EqHTTPResp(x.resp)));
}

std::vector<http::Record> GetTargetRecords(const types::ColumnWrapperRecordBatch& record_batch,
                                           int32_t pid) {
  std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, pid);
  return ToRecordVector(record_batch, target_record_indices);
}

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(DynLibTraceTest, TraceDynLoadedOpenSSL) {
  // Note that stirling is deployed before starting this test.

  // Makes the test run much faster.
  FLAGS_stirling_disable_self_tracing = true;

  NginxContainer server;
  RubyContainer client;
  DataTable data_table(kHTTPTable);

  {
    // Run the nginx HTTPS server.
    // The container runner will make sure it is in the ready state before unblocking.
    StatusOr<std::string> run_result = server.Run(60);
    PL_CHECK_OK(run_result);
  }

  // This TransferData will detect nginx for the first time, and deploy uprobes on its libssl.
  RefreshContext();
  source_->TransferData(ctx_.get(), SocketTraceConnector::kHTTPTableNum, &data_table);
  sleep(1);

  {
    // The key to this test is that Ruby only loads OpenSSL when it's required (i.e. http.request()
    // call), By sleeping at the beginning of the loop, Stirling will first detect the ruby binary
    // without OpenSSL.
    // Then we make multiple requests:
    //  - The first request will load the OpenSSL library, but won't be traced since the uprobes
    //  won't be deployed yet.
    //    This should cause the OpenSSL library to be dynamically loaded and then tracing should
    //    begin.
    //  - The subsequent requests should come after the uprobes are deployed and should be traced.
    std::string rb_script = R"(
          require 'net/http'
          require 'uri'

          $i = 0
          while $i < 3 do
            sleep(3)

            uri = URI.parse('https://localhost:443/index.html')
            http = Net::HTTP.new(uri.host, uri.port)
            http.use_ssl = true
            http.verify_mode = OpenSSL::SSL::VERIFY_NONE
            request = Net::HTTP::Get.new(uri.request_uri)
            response = http.request(request)
            p response.body

            $i += 1
          end
)";

    // Make an SSL request with the client.
    // Run the client in the network of the server, so they can connect to each other.
    PL_CHECK_OK(client.Run(10,
                           {absl::Substitute("--network=container:$0", server.container_name())},
                           {"ruby", "-e", rb_script}));

    // Periodically run TransferData.
    // Do this at a frequency faster than the sleep in the Ruby script.
    // This is to detect libssl, and deploy uprobes.
    for (int i = 0; i < 20; ++i) {
      RefreshContext();
      source_->TransferData(ctx_.get(), SocketTraceConnector::kHTTPTableNum, &data_table);
      sleep(1);
    }
    client.Wait();

    source_->TransferData(ctx_.get(), SocketTraceConnector::kHTTPTableNum, &data_table);
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
    ASSERT_FALSE(tablets.empty());
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

    // Inspect records for Debug.
    for (size_t i = 0; i < record_batch[0]->Size(); ++i) {
      uint32_t pid = record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(i).High64();
      std::string req_path = record_batch[kHTTPReqPathIdx]->Get<types::StringValue>(i);
      VLOG(1) << absl::Substitute("$0 $1", pid, req_path);
    }

    // Check server-side tracing results.
    {
      // Nginx has a master process and a worker process. We need the PID of the worker process.
      int worker_pid;
      std::string pid_str =
          pl::Exec(absl::Substitute("pgrep --parent $0", server.process_pid())).ValueOrDie();
      ASSERT_TRUE(absl::SimpleAtoi(pid_str, &worker_pid));
      LOG(INFO) << absl::Substitute("Worker thread PID: $0", worker_pid);

      std::vector<http::Record> records = GetTargetRecords(record_batch, worker_pid);

      http::Record expected_record;
      expected_record.req.minor_version = 1;
      expected_record.req.req_path = "/index.html";
      expected_record.resp.resp_status = 200;

      EXPECT_THAT(records,
                  UnorderedElementsAre(EqHTTPRecord(expected_record), EqHTTPRecord(expected_record),
                                       EqHTTPRecord(expected_record)));
    }

    // Check client-side tracing results.
    {
      std::vector<http::Record> records = GetTargetRecords(record_batch, client.process_pid());

      http::Record expected_record;
      expected_record.req.minor_version = 1;
      expected_record.req.req_path = "/index.html";
      expected_record.resp.resp_status = 200;

      EXPECT_THAT(records, UnorderedElementsAre(EqHTTPRecord(expected_record),
                                                EqHTTPRecord(expected_record)));
    }
  }
}

}  // namespace stirling
}  // namespace pl
