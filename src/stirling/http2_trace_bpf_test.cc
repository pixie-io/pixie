#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/http_table.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::FindRecordIdxMatchesPid;
using ::pl::testing::BazelBinTestFilePath;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

//-----------------------------------------------------------------------------
// Test Stimulus: Server and Client
//-----------------------------------------------------------------------------

class GRPCServerContainer : public ContainerRunner {
 public:
  GRPCServerContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "experimental/bpf/tracers/bin/go_grpc_tls_pl/server/server_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "grpc_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTP/2 server";
};

class GRPCClientContainer : public ContainerRunner {
 public:
  GRPCClientContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "experimental/bpf/tracers/bin/go_grpc_tls_pl/client/client_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "grpc_client";
  static constexpr std::string_view kReadyMessage = "";
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

//-----------------------------------------------------------------------------
// Test Class and Test Cases
//-----------------------------------------------------------------------------

class HTTP2TraceTest : public testing::SocketTraceBPFTest {
 protected:
  HTTP2TraceTest() {
    // Run the server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    // Note that this step will make an access to docker hub to  download the HTTPandra image.
    PL_CHECK_OK(server_.Run(60, {}));

    FLAGS_stirling_enable_parsing_protobufs = true;
  }
  ~HTTP2TraceTest() {
    client_.Stop();
    server_.Stop();
  }

  GRPCServerContainer server_;
  GRPCClientContainer client_;
};

TEST_F(HTTP2TraceTest, Basic) {
  // Run the server in the network of the server, so they can connect to each other.
  PL_CHECK_OK(
      client_.Run(10, {absl::Substitute("--network=container:$0", server_.container_name())}));

  // Sleep a little more, just to be safe.
  sleep(3);

  // Grab the data from Stirling.
  DataTable data_table(kHTTPTable);
  source_->TransferData(ctx_.get(), SocketTraceConnector::kHTTPTableNum, &data_table);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPid(record_batch, kHTTPUPIDIdx, server_.process_pid());

    // For Debug:
    for (const auto& idx : target_record_indices) {
      uint32_t pid = record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(idx).High64();
      std::string req_path = record_batch[kHTTPReqPathIdx]->Get<types::StringValue>(idx);
      std::string req_method = record_batch[kHTTPReqMethodIdx]->Get<types::StringValue>(idx);
      std::string req_body = record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(idx);

      int resp_status = record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(idx).val;
      std::string resp_message = record_batch[kHTTPRespMessageIdx]->Get<types::StringValue>(idx);
      std::string resp_body = record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(idx);
      VLOG(1) << absl::Substitute("$0 $1 $2 $3 $4 $5 $6", pid, req_method, req_path, req_body,
                                  resp_status, resp_message, resp_body);
    }

    std::vector<http::Record> records = ToRecordVector(record_batch, target_record_indices);

    // TODO(oazizi): Add headers checking too.
    http::Record expected_record = {};
    expected_record.req.req_path = "/pl.go_grpc_tls_pl.server.Greeter/SayHello";
    expected_record.req.req_method = "POST";
    expected_record.req.body = R"(1: "0")";
    expected_record.resp.resp_status = 200;
    expected_record.resp.resp_message = "OK";
    expected_record.resp.body = R"(1: "Hello 0")";

    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record)));
  }
}

}  // namespace stirling
}  // namespace pl
