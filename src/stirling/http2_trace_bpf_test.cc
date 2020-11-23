#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/http_table.h"
#include "src/stirling/output.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

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

class GRPCServerContainer : public ContainerRunner {
 public:
  GRPCServerContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "demos/client_server_apps/go_grpc_tls_pl/server/server_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "grpc_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTP/2 server";
};

class GRPCClientContainer : public ContainerRunner {
 public:
  GRPCClientContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "demos/client_server_apps/go_grpc_tls_pl/client/client_image.tar";
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

class HTTP2TraceTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  HTTP2TraceTest() {
    // Run the server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    // Note that this step will make an access to docker hub to download the HTTP image.
    PL_CHECK_OK(server_.Run(60, {}));

    FLAGS_stirling_enable_parsing_protobufs = true;
  }

  GRPCServerContainer server_;
  GRPCClientContainer client_;
};

TEST_F(HTTP2TraceTest, Basic) {
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

  EXPECT_THAT(FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, client_.process_pid()),
              IsEmpty());

  {
    DataTable data_table(kConnStatsTable);
    source_->TransferData(ctx_.get(), SocketTraceConnector::kConnStatsTableNum, &data_table);
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();

    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

    auto indices = FindRecordIdxMatchesPID(record_batch, kPGSQLUPIDIdx, server_.process_pid());
    ASSERT_THAT(indices, SizeIs(1));

    int conn_open =
        AccessRecordBatch<types::Int64Value>(record_batch, conn_stats_idx::kConnOpen, indices[0])
            .val;
    int conn_close =
        AccessRecordBatch<types::Int64Value>(record_batch, conn_stats_idx::kConnClose, indices[0])
            .val;
    int bytes_sent =
        AccessRecordBatch<types::Int64Value>(record_batch, conn_stats_idx::kBytesSent, indices[0])
            .val;
    int bytes_rcvd =
        AccessRecordBatch<types::Int64Value>(record_batch, conn_stats_idx::kBytesRecv, indices[0])
            .val;
    EXPECT_THAT(conn_open, 1);
    // TODO(oazizi/yzhao): Causing flakiness. Investigate.
    // EXPECT_THAT(conn_close, 1);
    PL_UNUSED(conn_close);
    EXPECT_THAT(bytes_sent, Gt(2000));
    EXPECT_THAT(bytes_rcvd, Gt(900));
  }
}

class ProductCatalogService : public ContainerRunner {
 public:
  ProductCatalogService() : ContainerRunner(kImage, kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImage =
      "gcr.io/google-samples/microservices-demo/productcatalogservice:v0.2.0";
  static constexpr std::string_view kInstanceNamePrefix = "pcs";
  static constexpr std::string_view kReadyMessage = "starting grpc server";
};

class ProductCatalogClient : public ContainerRunner {
 public:
  ProductCatalogClient()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "demos/applications/hipster_shop/productcatalogservice_client/"
      "productcatalogservice_client_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "pcc";
  static constexpr std::string_view kReadyMessage = "";
};

class ProductCatalogServiceTraceTest
    : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  ProductCatalogServiceTraceTest() {
    // Run the server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    // Note that this step will make an access to docker hub to  download the HTTP image.
    PL_CHECK_OK(server_.Run(60, {}));

    FLAGS_stirling_enable_parsing_protobufs = true;
  }

  ProductCatalogService server_;
  ProductCatalogClient client_;
};

TEST_F(ProductCatalogServiceTraceTest, Basic) {
  // Run the client in the network of the server, so they can connect to each other.
  PL_CHECK_OK(
      client_.Run(10, {absl::Substitute("--network=container:$0", server_.container_name())}));
  client_.Wait();

  // We do not expect this sleep to be required, but it appears to be necessary for Jenkins.
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

    EXPECT_EQ(records.size(), 3);

    http::Record expected_record1 = {};
    expected_record1.req.req_path = "/hipstershop.ProductCatalogService/ListProducts";
    expected_record1.req.req_method = "POST";
    expected_record1.req.body = R"()";
    expected_record1.resp.resp_status = 200;
    expected_record1.resp.resp_message = "OK";
    expected_record1.resp.body = R"(products {
  id: "OLJCESPC7Z"
  name: "Vintage Typewriter"
  description: "This typewriter looks good in your living room."
  picture: "/static/img/products/typewriter.jpg"
  price_usd {
    currency_code: "USD"
    units: 67
    nanos: 990000000
  }
  categories: "vintage"
}
products {
  id: "66VCHSJNUP"
  name: "Vintage Camera Lens"
  description: "You won\'t have a camera to use it and it probably doesn\'t work anyway."
  picture: "/static/img/products/camera-lens.jpg"
  price_usd {
    currency_code: "U... [TRUNCATED])";

    http::Record expected_record2 = {};
    expected_record2.req.req_path = "/hipstershop.ProductCatalogService/GetProduct";
    expected_record2.req.req_method = "POST";
    expected_record2.req.body = R"(id: "OLJCESPC7Z")";
    expected_record2.resp.resp_status = 200;
    expected_record2.resp.resp_message = "OK";
    expected_record2.resp.body = R"(id: "OLJCESPC7Z"
name: "Vintage Typewriter"
description: "This typewriter looks good in your living room."
picture: "/static/img/products/typewriter.jpg"
price_usd {
  currency_code: "USD"
  units: 67
  nanos: 990000000
}
categories: "vintage")";

    http::Record expected_record3 = {};
    expected_record3.req.req_path = "/hipstershop.ProductCatalogService/SearchProducts";
    expected_record3.req.req_method = "POST";
    expected_record3.req.body = R"(query: "typewriter")";
    expected_record3.resp.resp_status = 200;
    expected_record3.resp.resp_message = "OK";
    expected_record3.resp.body = R"(results {
  id: "OLJCESPC7Z"
  name: "Vintage Typewriter"
  description: "This typewriter looks good in your living room."
  picture: "/static/img/products/typewriter.jpg"
  price_usd {
    currency_code: "USD"
    units: 67
    nanos: 990000000
  }
  categories: "vintage"
})";

    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record1)));
    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record2)));
    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record3)));
  }
}

}  // namespace stirling
}  // namespace pl
