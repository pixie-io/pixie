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

#include <future>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <sole.hpp>

#include "src/api/proto/vizierpb/vizierapi.grpc.pb.h"
#include "src/carnot/carnot.h"
#include "src/carnot/exec/local_grpc_result_server.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/funcs/funcs.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/services/jwt/service_token.h"
#include "src/table_store/table_store.h"
#include "src/vizier/services/agent/pem/direct_query_server.h"

namespace px {
namespace vizier {
namespace agent {

using ::px::api::vizierpb::ExecuteScriptRequest;
using ::px::api::vizierpb::ExecuteScriptResponse;
using ::px::api::vizierpb::VizierService;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

constexpr char kTestSigningKey[] = "test-signing-key";
constexpr char kSimpleQuery[] = "import px\npx.display(px.DataFrame('test_table'), 'out')";
// Rows in CarnotTestUtils::TestTable().
constexpr int64_t kTestTableRows = 5;

std::string MakeToken(const std::string& signing_key) {
  return ::px::services::GenerateServiceToken(signing_key, "direct_query_test").ConsumeValueOrDie();
}

struct QueryResult {
  ::grpc::Status status;
  std::vector<ExecuteScriptResponse> responses;

  std::vector<const ::px::api::vizierpb::QueryMetadata*> Metadata() const {
    std::vector<const ::px::api::vizierpb::QueryMetadata*> out;
    for (const auto& r : responses) {
      if (r.has_meta_data()) {
        out.push_back(&r.meta_data());
      }
    }
    return out;
  }

  // Sums num_rows over every data batch belonging to the table with this display name.
  int64_t NumRows(const std::string& table_name) const {
    std::string table_id;
    for (const auto* md : Metadata()) {
      if (md->name() == table_name) {
        table_id = md->id();
      }
    }
    int64_t rows = 0;
    for (const auto& r : responses) {
      if (r.has_data() && r.data().has_batch() && r.data().batch().table_id() == table_id) {
        rows += r.data().batch().num_rows();
      }
    }
    return rows;
  }
};

class DirectQueryServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    table_store_ = std::make_shared<::px::table_store::TableStore>();
    table_store_->AddTable("test_table", ::px::carnot::exec::CarnotTestUtils::TestTable());
    table_store_->AddTable("duration_table",
                           ::px::carnot::exec::CarnotTestUtils::TestDuration64Table());

    ASSERT_OK_AND_ASSIGN(result_server_, ::px::carnot::exec::LocalGRPCResultSinkServer::Create());

    auto func_registry = std::make_unique<::px::carnot::udf::Registry>("direct_query_test");
    ::px::carnot::funcs::RegisterFuncsOrDie(func_registry.get());
    auto clients_config =
        std::make_unique<::px::carnot::Carnot::ClientsConfig>(::px::carnot::Carnot::ClientsConfig{
            [this](const std::string& address, const std::string&) {
              return result_server_->StubGenerator(address);
            },
            [](::grpc::ClientContext*) {},
        });
    auto server_config = std::make_unique<::px::carnot::Carnot::ServerConfig>();
    server_config->grpc_server_creds = ::grpc::InsecureServerCredentials();
    server_config->grpc_server_port = 0;
    ASSERT_OK_AND_ASSIGN(
        carnot_, ::px::carnot::Carnot::Create(sole::uuid4(), std::move(func_registry), table_store_,
                                              std::move(clients_config), std::move(server_config)));

    service_ = std::make_unique<DirectQueryServer>(carnot_.get(), carnot_->GetEngineState(),
                                                   result_server_.get(), kTestSigningKey);
    ::grpc::ServerBuilder builder;
    builder.RegisterService(service_.get());
    server_ = builder.BuildAndStart();
    stub_ = VizierService::NewStub(server_->InProcessChannel({}));
  }

  void TearDown() override {
    if (server_) {
      server_->Shutdown();
    }
  }

  // Sends each entry of `authorization` as its own metadata value.
  QueryResult Execute(const std::string& pxl, const std::vector<std::string>& authorization,
                      bool mutation = false) {
    ::grpc::ClientContext ctx;
    for (const auto& value : authorization) {
      ctx.AddMetadata("authorization", value);
    }
    ExecuteScriptRequest req;
    req.set_query_str(pxl);
    req.set_mutation(mutation);
    auto reader = stub_->ExecuteScript(&ctx, req);
    QueryResult out;
    ExecuteScriptResponse resp;
    while (reader->Read(&resp)) {
      out.responses.push_back(resp);
    }
    out.status = reader->Finish();
    return out;
  }

  QueryResult Execute(const std::string& pxl) {
    return Execute(pxl, {"Bearer " + MakeToken(kTestSigningKey)});
  }

  std::shared_ptr<::px::table_store::TableStore> table_store_;
  std::unique_ptr<::px::carnot::exec::LocalGRPCResultSinkServer> result_server_;
  std::unique_ptr<::px::carnot::Carnot> carnot_;
  std::unique_ptr<DirectQueryServer> service_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<VizierService::Stub> stub_;
};

TEST_F(DirectQueryServerTest, MissingToken_Unauthenticated) {
  auto res = Execute(kSimpleQuery, {});
  EXPECT_EQ(::grpc::StatusCode::UNAUTHENTICATED, res.status.error_code());
  EXPECT_TRUE(res.responses.empty());
}

TEST_F(DirectQueryServerTest, TokenSignedWithOtherKey_Unauthenticated) {
  auto res = Execute(kSimpleQuery, {"Bearer " + MakeToken("a-different-key")});
  EXPECT_EQ(::grpc::StatusCode::UNAUTHENTICATED, res.status.error_code());
  EXPECT_TRUE(res.responses.empty());
}

TEST_F(DirectQueryServerTest, NonBearerScheme_Unauthenticated) {
  auto res = Execute(kSimpleQuery, {"Token " + MakeToken(kTestSigningKey)});
  EXPECT_EQ(::grpc::StatusCode::UNAUTHENTICATED, res.status.error_code());
}

TEST_F(DirectQueryServerTest, DuplicateAuthorizationHeader_Unauthenticated) {
  const auto bearer = "Bearer " + MakeToken(kTestSigningKey);
  auto res = Execute(kSimpleQuery, {bearer, bearer});
  EXPECT_EQ(::grpc::StatusCode::UNAUTHENTICATED, res.status.error_code());
}

TEST_F(DirectQueryServerTest, Mutation_Unimplemented) {
  auto res = Execute(kSimpleQuery, {"Bearer " + MakeToken(kTestSigningKey)}, /* mutation */ true);
  EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, res.status.error_code());
}

TEST_F(DirectQueryServerTest, StreamsSchemaThenRows) {
  auto res = Execute(kSimpleQuery);
  ASSERT_TRUE(res.status.ok()) << res.status.error_message();
  ASSERT_FALSE(res.responses.empty());

  ASSERT_TRUE(res.responses[0].has_meta_data()) << "schema must precede data";
  const auto& md = res.responses[0].meta_data();
  EXPECT_EQ("out", md.name());
  ASSERT_EQ(2, md.relation().columns_size());
  EXPECT_EQ("col1", md.relation().columns(0).column_name());
  EXPECT_EQ("col2", md.relation().columns(1).column_name());

  EXPECT_EQ(kTestTableRows, res.NumRows("out"));

  const auto& query_id = res.responses[0].query_id();
  EXPECT_FALSE(query_id.empty());
  for (const auto& r : res.responses) {
    EXPECT_EQ(query_id, r.query_id());
  }
}

TEST_F(DirectQueryServerTest, StreamsColumnValues) {
  auto res = Execute(
      "import px\n"
      "df = px.DataFrame('test_table')\n"
      "px.display(df[['col2']], 'out')\n");
  ASSERT_TRUE(res.status.ok()) << res.status.error_message();

  std::vector<int64_t> values;
  for (const auto& r : res.responses) {
    if (!r.has_data() || !r.data().has_batch()) continue;
    for (const auto& col : r.data().batch().cols()) {
      ASSERT_TRUE(col.has_int64_data());
      values.insert(values.end(), col.int64_data().data().begin(), col.int64_data().data().end());
    }
  }
  EXPECT_THAT(values, ElementsAre(1, 2, 3, 5, 6));
}

TEST_F(DirectQueryServerTest, MultipleDisplays_StreamEachTable) {
  auto res = Execute(
      "import px\n"
      "px.display(px.DataFrame('test_table'), 't1')\n"
      "px.display(px.DataFrame('duration_table'), 't2')\n");
  ASSERT_TRUE(res.status.ok()) << res.status.error_message();

  std::vector<std::string> names;
  for (const auto* md : res.Metadata()) names.push_back(md->name());
  EXPECT_THAT(names, UnorderedElementsAre("t1", "t2"));
  EXPECT_EQ(kTestTableRows, res.NumRows("t1"));
  EXPECT_GT(res.NumRows("t2"), 0);
}

TEST_F(DirectQueryServerTest, MalformedPxL_InvalidArgument) {
  auto res = Execute("not valid python at all !!!");
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, res.status.error_code());
}

TEST_F(DirectQueryServerTest, NonexistentTable_InvalidArgument) {
  auto res = Execute("import px\npx.display(px.DataFrame('no_such_table'))");
  EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, res.status.error_code());
}

TEST_F(DirectQueryServerTest, SequentialQueries_DoNotAccumulateRows) {
  for (int i = 0; i < 3; ++i) {
    auto res = Execute(kSimpleQuery);
    ASSERT_TRUE(res.status.ok()) << "iteration " << i << ": " << res.status.error_message();
    EXPECT_EQ(kTestTableRows, res.NumRows("out")) << "iteration " << i;
  }
}

TEST_F(DirectQueryServerTest, ConcurrentQueries_DoNotMixRows) {
  constexpr int kNumQueries = 8;
  std::vector<std::future<QueryResult>> futures;
  for (int i = 0; i < kNumQueries; ++i) {
    futures.push_back(std::async(std::launch::async, [this]() { return Execute(kSimpleQuery); }));
  }
  for (auto& f : futures) {
    auto res = f.get();
    ASSERT_TRUE(res.status.ok()) << res.status.error_message();
    EXPECT_EQ(kTestTableRows, res.NumRows("out"));
  }
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
