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

#include "src/common/base/test_utils.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/output.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;

using ::px::testing::BazelBinTestFilePath;

using ::px::stirling::testing::AccessRecordBatch;
using ::px::stirling::testing::EqHTTPRecord;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::ToRecordVector;

using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::SizeIs;
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
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/server_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "grpc_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTP/2 server";
};

class GRPCClientContainer : public ContainerRunner {
 public:
  GRPCClientContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/client/client_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "grpc_client";
  static constexpr std::string_view kReadyMessage = "";
};

//-----------------------------------------------------------------------------
// Test Class and Test Cases
//-----------------------------------------------------------------------------

class HTTP2TraceTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  HTTP2TraceTest() {
    // Run the server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    PL_CHECK_OK(server_.Run(std::chrono::seconds{60}));
  }

  GRPCServerContainer server_;
  GRPCClientContainer client_;
};

TEST_F(HTTP2TraceTest, Basic) {
  StartTransferDataThread();

  // Run the client in the network of the server, so they can connect to each other.
  PL_CHECK_OK(client_.Run(std::chrono::seconds{10},
                          {absl::Substitute("--network=container:$0", server_.container_name())}));
  client_.Wait();

  StopTransferDataThread();

  {
    // Grab the data from Stirling.
    std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
    ASSERT_FALSE(tablets.empty());
    types::ColumnWrapperRecordBatch rb = tablets[0].records;

    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPID(rb, kHTTPUPIDIdx, server_.process_pid());

    // For Debug:
    for (const auto& idx : target_record_indices) {
      uint32_t pid = rb[kHTTPUPIDIdx]->Get<types::UInt128Value>(idx).High64();
      std::string req_path = rb[kHTTPReqPathIdx]->Get<types::StringValue>(idx);
      std::string req_method = rb[kHTTPReqMethodIdx]->Get<types::StringValue>(idx);
      std::string req_body = rb[kHTTPReqBodyIdx]->Get<types::StringValue>(idx);

      int resp_status = rb[kHTTPRespStatusIdx]->Get<types::Int64Value>(idx).val;
      std::string resp_message = rb[kHTTPRespMessageIdx]->Get<types::StringValue>(idx);
      std::string resp_body = rb[kHTTPRespBodyIdx]->Get<types::StringValue>(idx);
      VLOG(1) << absl::Substitute("$0 $1 $2 $3 $4 $5 $6", pid, req_method, req_path, req_body,
                                  resp_status, resp_message, resp_body);
    }

    std::vector<http::Record> records = ToRecordVector(rb, target_record_indices);

    // TODO(oazizi): Add headers checking too.
    http::Record expected_record = {};
    expected_record.req.req_path = "/px.go_grpc_tls_pl.server.Greeter/SayHello";
    expected_record.req.req_method = "POST";
    expected_record.req.body = R"(1: "0")";
    expected_record.resp.resp_status = 200;
    expected_record.resp.resp_message = "OK";
    expected_record.resp.body = R"(1: "Hello 0")";

    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record)));

    EXPECT_THAT(FindRecordIdxMatchesPID(rb, kHTTPUPIDIdx, client_.process_pid()), IsEmpty());
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
      "src/stirling/testing/demo_apps/hipster_shop/productcatalogservice_client/"
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
    // Note that this step will make an access to docker hub to download the HTTP image.
    PL_CHECK_OK(server_.Run(std::chrono::seconds{60}));
  }

  ProductCatalogService server_;
  ProductCatalogClient client_;
};

TEST_F(ProductCatalogServiceTraceTest, Basic) {
  StartTransferDataThread();

  // Run the client in the network of the server, so they can connect to each other.
  PL_CHECK_OK(client_.Run(std::chrono::seconds{10},
                          {absl::Substitute("--network=container:$0", server_.container_name())}));
  client_.Wait();

  StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_FALSE(tablets.empty());
  const types::ColumnWrapperRecordBatch& rb = tablets[0].records;

  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(rb, kHTTPUPIDIdx, server_.process_pid());

  std::vector<size_t> req_body_sizes;
  std::vector<size_t> resp_body_sizes;

  for (const auto& idx : target_record_indices) {
    req_body_sizes.push_back(
        AccessRecordBatch<types::Int64Value>(rb, kHTTPReqBodySizeIdx, idx).val);
    resp_body_sizes.push_back(
        AccessRecordBatch<types::Int64Value>(rb, kHTTPRespBodySizeIdx, idx).val);
  }
  EXPECT_THAT(req_body_sizes, UnorderedElementsAre(5, 17, 17));
  EXPECT_THAT(resp_body_sizes, UnorderedElementsAre(147, 150, 1439));

  {
    std::vector<http::Record> records = ToRecordVector(rb, target_record_indices);

    EXPECT_THAT(records, SizeIs(3));

    http::Record expected_record1 = {};
    expected_record1.req.req_path = "/hipstershop.ProductCatalogService/ListProducts";
    expected_record1.req.req_method = "POST";
    expected_record1.req.body = R"()";
    expected_record1.resp.resp_status = 200;
    expected_record1.resp.resp_message = "OK";

    // Note that the truncation is applied in 2 places below:
    // 1. Inside string parsing, where the field #1 has a string truncated.
    // 2. The whole message was truncated as well.
    expected_record1.resp.body = R"(1 {
  1: "OLJCESPC7Z"
  2: "Vintage Typewriter"
  3: "This typewriter looks good in your living room."
  4: "/static/img/products/typewriter.jpg"
  5 {
    1: "USD"
    2: 67
    3: 990000000
  }
  6: "vintage"
}
1 {
  1: "66VCHSJNUP"
  2: "Vintage Camera Lens"
  3: "You won\'t have a camera to use it and it probably doesn\'t work a...<truncated>..."
  4: "/static/img/products/camera-lens.jpg"
  5 {
    1: "USD"
    2: 12
    3: 490000000
  }
  6: "photography"
  6: "vintage"
}
1 {
  1: "1YMWWN1N4O"
  2: "Home Barista Kit"
  3: "Always wanted to brew coffee with Chemex and Aeropress at home?"
  4: "/static/img/products/barista-kit.jpg"
  5 {
    1: "USD"
    2: 124
  }
  6: "cookware"
}
1: "\n\nL9ECAV7KIM\022\tTerrari"... [TRUNCATED])";

    http::Record expected_record2 = {};
    expected_record2.req.req_path = "/hipstershop.ProductCatalogService/GetProduct";
    expected_record2.req.req_method = "POST";
    expected_record2.req.body = R"(1: "OLJCESPC7Z")";
    expected_record2.resp.resp_status = 200;
    expected_record2.resp.resp_message = "OK";
    expected_record2.resp.body = R"(1: "OLJCESPC7Z"
2: "Vintage Typewriter"
3: "This typewriter looks good in your living room."
4: "/static/img/products/typewriter.jpg"
5 {
  1: "USD"
  2: 67
  3: 990000000
}
6: "vintage")";

    http::Record expected_record3 = {};
    expected_record3.req.req_path = "/hipstershop.ProductCatalogService/SearchProducts";
    expected_record3.req.req_method = "POST";
    expected_record3.req.body = R"(1: "typewriter")";
    expected_record3.resp.resp_status = 200;
    expected_record3.resp.resp_message = "OK";
    expected_record3.resp.body = R"(1 {
  1: "OLJCESPC7Z"
  2: "Vintage Typewriter"
  3: "This typewriter looks good in your living room."
  4: "/static/img/products/typewriter.jpg"
  5 {
    1: "USD"
    2: 67
    3: 990000000
  }
  6: "vintage"
})";

    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record1)));
    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record2)));
    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record3)));
  }
}

// TODO(yzhao): Add test for bidirectional streaming

}  // namespace stirling
}  // namespace px
