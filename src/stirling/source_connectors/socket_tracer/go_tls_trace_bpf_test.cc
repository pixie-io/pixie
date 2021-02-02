#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {

// Automatically converts ToString() to stream operator for gtest.
using ::pl::operator<<;

namespace http = protocols::http;

using ::pl::stirling::testing::AccessRecordBatch;
using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::testing::BazelBinTestFilePath;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;
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
      "demos/client_server_apps/go_https/server/https_server_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "https_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTPS service";
};

class GoTLSClientContainer : public ContainerRunner {
 public:
  GoTLSClientContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "demos/client_server_apps/go_https/client/https_client_image.tar";
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
    // Note that this step will make an access to docker hub to download the HTTP image.
    PL_CHECK_OK(server_.Run(60, {}));
  }

  GoTLSServerContainer server_;
  GoTLSClientContainer client_;
};

//-----------------------------------------------------------------------------
// Result Checking: Helper Functions and Matchers
//-----------------------------------------------------------------------------

std::vector<http::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                         const std::vector<size_t>& indices) {
  std::vector<http::Record> result;

  for (const auto& idx : indices) {
    http::Record r;
    r.req.req_path = rb[kHTTPReqPathIdx]->Get<types::StringValue>(idx);
    r.req.req_method = rb[kHTTPReqMethodIdx]->Get<types::StringValue>(idx);
    r.req.body = rb[kHTTPReqBodyIdx]->Get<types::StringValue>(idx);

    r.resp.resp_status = rb[kHTTPRespStatusIdx]->Get<types::Int64Value>(idx).val;
    r.resp.resp_message = rb[kHTTPRespMessageIdx]->Get<types::StringValue>(idx);
    r.resp.body = rb[kHTTPRespBodyIdx]->Get<types::StringValue>(idx);

    result.push_back(r);
  }
  return result;
}

auto EqHTTPReq(const http::Message& x) {
  return AllOf(Field(&http::Message::req_path, Eq(x.req_path)),
               Field(&http::Message::req_method, StrEq(x.req_method)),
               Field(&http::Message::body, StrEq(x.body)));
}

auto EqHTTPResp(const http::Message& x) {
  return AllOf(Field(&http::Message::resp_status, Eq(x.resp_status)),
               Field(&http::Message::resp_message, StrEq(x.resp_message)),
               Field(&http::Message::body, StrEq(x.body)));
}

auto EqHTTPRecord(const http::Record& x) {
  return AllOf(Field(&http::Record::req, EqHTTPReq(x.req)),
               Field(&http::Record::resp, EqHTTPResp(x.resp)));
}

TEST_F(GoTLSTraceTest, Basic) {
  // Run the client in the network of the server, so they can connect to each other.
  PL_CHECK_OK(
      client_.Run(10, {absl::Substitute("--network=container:$0", server_.container_name())}));
  client_.Wait();

  // We do not expect this sleep to be required, but it appears to be necessary for Jenkins.
  // TODO(oazizi): Figure out why.
  sleep(3);

  // Grab the data from Stirling.
  DataTable data_table(kHTTPTable);
  source_->TransferData(ctx_.get(), SocketTraceConnector::kHTTPTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, server_.process_pid());

    // For Debug:
    for (const auto& idx : target_record_indices) {
      uint32_t pid = record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(idx).High64();
      std::string req_path = record_batch[kHTTPReqPathIdx]->Get<types::StringValue>(idx);
      std::string req_method = record_batch[kHTTPReqMethodIdx]->Get<types::StringValue>(idx);
      std::string req_body = record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(idx);

      int resp_status = record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(idx).val;
      std::string resp_message = record_batch[kHTTPRespMessageIdx]->Get<types::StringValue>(idx);
      std::string resp_body = record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(idx);
      LOG(INFO) << absl::Substitute("$0 $1 $2 $3 $4 $5 $6", pid, req_method, req_path, req_body,
                                    resp_status, resp_message, resp_body);
    }

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
}  // namespace pl
