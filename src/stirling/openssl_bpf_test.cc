#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include <absl/strings/str_replace.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/data_table.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

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
  static constexpr std::string_view kBazelImageTar = "src/stirling/testing/ssl/nginx_ssl_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "nginx";
  static constexpr std::string_view kReadyMessage = "";
};

class CurlContainer : public ContainerRunner {
 public:
  CurlContainer() : ContainerRunner(kImageName, kContainerNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImageName = "curlimages/curl:7.74.0";
  static constexpr std::string_view kContainerNamePrefix = "curl";
  static constexpr std::string_view kReadyMessage = "";
};

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

  NginxContainer server_;
  CurlContainer client_;
};

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

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

std::vector<http::Record> GetTargetRecords(const types::ColumnWrapperRecordBatch& record_batch,
                                           int32_t pid) {
  std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, pid);
  return ToRecordVector(record_batch, target_record_indices);
}

TEST_F(OpenSSLTraceTest, ssl_capture) {
  {
    // Make an SSL request with curl.
    // Because the server uses a self-signed certificate, curl will normally refuse to connect.
    // This is similar to the warning pages that Firefox/Chrome would display.
    // To take an exception and make the SSL connection anyways, we use the --insecure flag.

    // Run the client in the network of the server, so they can connect to each other.
    PL_CHECK_OK(client_.Run(10,
                            {absl::Substitute("--network=container:$0", server_.container_name())},
                            {"--insecure", "-s", "-S", "https://localhost:443/index.html"}));
    client_.Wait();

    // Grab the data from Stirling.
    DataTable data_table(kHTTPTable);
    source_->TransferData(ctx_.get(), SocketTraceConnector::kHTTPTableNum, &data_table);
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
    ASSERT_FALSE(tablets.empty());
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

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
          pl::Exec(absl::Substitute("pgrep -P $0", server_.process_pid())).ValueOrDie();
      ASSERT_TRUE(absl::SimpleAtoi(pid_str, &worker_pid));
      LOG(INFO) << absl::Substitute("Worker thread PID: $0", worker_pid);

      std::vector<http::Record> records = GetTargetRecords(record_batch, worker_pid);

      http::Record expected_record;
      expected_record.req.minor_version = 1;
      expected_record.req.req_path = "/index.html";
      expected_record.resp.resp_status = 200;

      EXPECT_THAT(records, UnorderedElementsAre(EqHTTPRecord(expected_record)));
    }
  }
}

}  // namespace stirling
}  // namespace pl
