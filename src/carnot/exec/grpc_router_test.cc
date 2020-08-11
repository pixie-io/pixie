#include "src/carnot/exec/grpc_router.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <utility>

#include <absl/strings/substitute.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_impl.h>
#include <gtest/gtest.h>

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/grpc_source_node.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/common/uuid/proto/uuid.pb.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"
#include "src/table_store/proto/schema.pb.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

using grpc::Channel;
using grpc::InsecureChannelCredentials;
using grpc::InsecureServerCredentials;
using grpc::Server;
using grpc::ServerBuilder;
using table_store::schema::RowDescriptor;

class GRPCRouterTest : public ::testing::Test {
 protected:
  GRPCRouterTest() { service_ = std::make_unique<GRPCRouter>(); }

  void SetUp() override {
    // Setup server.
    ServerBuilder builder;
    builder.AddListeningPort(GetServerAddress(), InsecureServerCredentials(), &port_);
    builder.RegisterService(service_.get());
    server_ = builder.BuildAndStart();
  }

  void TearDown() override { server_->Shutdown(); }

  void ResetStub() {
    std::shared_ptr<Channel> channel =
        grpc::CreateChannel(GetServerAddress(), InsecureChannelCredentials());
    stub_ = carnotpb::ResultSinkService::NewStub(channel);
  }

  std::string GetServerAddress() { return absl::Substitute("$0:$1", hostname, port_); }

  std::unique_ptr<carnotpb::ResultSinkService::Stub> stub_;
  std::unique_ptr<Server> server_;
  std::string hostname = "0.0.0.0";
  int port_ = 0;
  std::unique_ptr<GRPCRouter> service_;
};

class FakeGRPCSourceNode : public pl::carnot::exec::GRPCSourceNode {
 public:
  Status EnqueueRowBatch(std::unique_ptr<pl::carnotpb::TransferResultChunkRequest> row_batch) {
    row_batches.emplace_back(std::move(row_batch));

    return Status::OK();
  }

  std::vector<std::unique_ptr<pl::carnotpb::TransferResultChunkRequest>> row_batches;
};

TEST_F(GRPCRouterTest, no_node_router_test) {
  int64_t grpc_source_node_id = 1;
  ResetStub();
  auto query_id_str = "ea8aa095-697f-49f1-b127-d50e5b6e2645";

  // Create row batches.
  RowDescriptor input_rd({types::DataType::INT64});
  auto rb1 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({1, 2})
                 .get();
  carnotpb::TransferResultChunkRequest rb_req1;
  EXPECT_OK(rb1.ToProto(rb_req1.mutable_row_batch_result()->mutable_row_batch()));
  rb_req1.set_address(hostname);
  rb_req1.mutable_row_batch_result()->set_grpc_source_id(grpc_source_node_id);
  auto query_id = rb_req1.mutable_query_id();
  query_id->set_data(query_id_str);

  auto rb2 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({4, 6})
                 .get();
  carnotpb::TransferResultChunkRequest rb_req2;
  EXPECT_OK(rb2.ToProto(rb_req2.mutable_row_batch_result()->mutable_row_batch()));
  rb_req2.set_address(hostname);
  rb_req2.mutable_row_batch_result()->set_grpc_source_id(grpc_source_node_id);
  query_id = rb_req2.mutable_query_id();
  query_id->set_data(query_id_str);

  // Send row batches to GRPC router.
  carnotpb::TransferResultChunkResponse response;
  grpc::ClientContext context;
  auto writer = stub_->TransferResultChunk(&context, &response);
  writer->Write(rb_req1);
  writer->Write(rb_req2);
  writer->WritesDone();
  writer->Finish();

  // Add source node to GRPC router.
  auto query_uuid = sole::rebuild(query_id_str);
  auto op_proto = planpb::testutils::CreateTestGRPCSource1PB();
  std::unique_ptr<pl::carnot::plan::Operator> plan_node =
      plan::GRPCSourceOperator::FromProto(op_proto, grpc_source_node_id);
  auto source_node = FakeGRPCSourceNode();
  ASSERT_OK(source_node.Init(*plan_node, input_rd, {}));

  auto s = service_->AddGRPCSourceNode(query_uuid, grpc_source_node_id, &source_node, [] {});
  ASSERT_OK(s);
  EXPECT_EQ(2, source_node.row_batches.size());
  EXPECT_EQ(
      1,
      source_node.row_batches.at(0)->row_batch_result().row_batch().cols(0).int64_data().data(0));
  EXPECT_EQ(
      4,
      source_node.row_batches.at(1)->row_batch_result().row_batch().cols(0).int64_data().data(0));
}

TEST_F(GRPCRouterTest, basic_router_test) {
  int64_t grpc_source_node_id = 1;
  ResetStub();
  auto query_id_str = "ea8aa095-697f-49f1-b127-d50e5b6e2645";

  RowDescriptor input_rd({types::DataType::INT64});
  auto query_uuid = sole::rebuild(query_id_str);

  // Add source node to GRPC router.
  auto op_proto = planpb::testutils::CreateTestGRPCSource1PB();
  std::unique_ptr<pl::carnot::plan::Operator> plan_node =
      plan::GRPCSourceOperator::FromProto(op_proto, grpc_source_node_id);
  auto source_node = FakeGRPCSourceNode();
  ASSERT_OK(source_node.Init(*plan_node, input_rd, {}));

  auto num_continues = 0;
  auto s = service_->AddGRPCSourceNode(query_uuid, grpc_source_node_id, &source_node,
                                       [&] { num_continues++; });
  ASSERT_OK(s);
  EXPECT_EQ(0, source_node.row_batches.size());
  EXPECT_EQ(0, num_continues);

  // Create row batches.
  auto rb1 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({1, 2})
                 .get();
  carnotpb::TransferResultChunkRequest rb_req1;
  EXPECT_OK(rb1.ToProto(rb_req1.mutable_row_batch_result()->mutable_row_batch()));
  rb_req1.set_address(hostname);
  rb_req1.mutable_row_batch_result()->set_grpc_source_id(grpc_source_node_id);
  auto query_id = rb_req1.mutable_query_id();
  query_id->set_data(query_id_str);

  auto rb2 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({4, 6})
                 .get();
  carnotpb::TransferResultChunkRequest rb_req2;
  EXPECT_OK(rb2.ToProto(rb_req2.mutable_row_batch_result()->mutable_row_batch()));
  rb_req2.set_address(hostname);
  rb_req2.mutable_row_batch_result()->set_grpc_source_id(grpc_source_node_id);
  query_id = rb_req2.mutable_query_id();
  query_id->set_data(query_id_str);

  // Send row batches to GRPC router.
  pl::carnotpb::TransferResultChunkResponse response;
  grpc::ClientContext context;
  auto writer = stub_->TransferResultChunk(&context, &response);
  writer->Write(rb_req1);
  writer->Write(rb_req2);
  writer->WritesDone();
  writer->Finish();

  EXPECT_EQ(2, source_node.row_batches.size());
  EXPECT_EQ(
      1,
      source_node.row_batches.at(0)->row_batch_result().row_batch().cols(0).int64_data().data(0));
  EXPECT_EQ(
      4,
      source_node.row_batches.at(1)->row_batch_result().row_batch().cols(0).int64_data().data(0));
  EXPECT_EQ(2, num_continues);
}

TEST_F(GRPCRouterTest, router_and_done_test) {
  int64_t grpc_source_node_id = 1;
  ResetStub();
  auto query_id_str = "ea8aa095-697f-49f1-b127-d50e5b6e2645";
  auto query_uuid = sole::rebuild(query_id_str);

  auto agent_id_str = "e302d3f9-f20a-44a3-bdc5-36fc14ed9089";
  auto agent_uuid = sole::rebuild(agent_id_str);

  RowDescriptor input_rd({types::DataType::INT64});

  // Add source node to GRPC router.
  auto op_proto = planpb::testutils::CreateTestGRPCSource1PB();
  std::unique_ptr<pl::carnot::plan::Operator> plan_node =
      plan::GRPCSourceOperator::FromProto(op_proto, grpc_source_node_id);
  auto source_node = FakeGRPCSourceNode();
  ASSERT_OK(source_node.Init(*plan_node, input_rd, {}));

  auto num_continues = 0;
  auto s = service_->AddGRPCSourceNode(query_uuid, grpc_source_node_id, &source_node,
                                       [&] { num_continues++; });
  ASSERT_OK(s);
  EXPECT_EQ(0, source_node.row_batches.size());
  EXPECT_EQ(0, num_continues);

  // Create row batch.
  auto rb1 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({1, 2})
                 .get();
  carnotpb::TransferResultChunkRequest rb_req1;
  EXPECT_OK(rb1.ToProto(rb_req1.mutable_row_batch_result()->mutable_row_batch()));
  rb_req1.set_address(hostname);
  rb_req1.mutable_row_batch_result()->set_grpc_source_id(grpc_source_node_id);
  auto query_id = rb_req1.mutable_query_id();
  query_id->set_data(query_id_str);

  // Send row batches to GRPC router.
  pl::carnotpb::TransferResultChunkResponse response;
  grpc::ClientContext context;
  auto writer = stub_->TransferResultChunk(&context, &response);
  writer->Write(rb_req1);
  writer->WritesDone();
  writer->Finish();

  {
    ::pl::carnotpb::DoneRequest done_req;
    ::pl::carnotpb::DoneResponse done_resp;

    auto agent_stats = done_req.add_agent_execution_stats();
    ToProto(agent_uuid, agent_stats->mutable_agent_id());
    ToProto(query_uuid, done_req.mutable_query_id());
    auto mem_src_stats = agent_stats->add_operator_execution_stats();
    mem_src_stats->set_bytes_output(123);
    mem_src_stats->set_records_output(1);
    mem_src_stats->set_total_execution_time_ns(10000);
    mem_src_stats->set_self_execution_time_ns(5000);

    auto grpc_sink_stats = agent_stats->add_operator_execution_stats();
    grpc_sink_stats->set_bytes_output(0);
    grpc_sink_stats->set_records_output(0);
    grpc_sink_stats->set_total_execution_time_ns(5000);
    grpc_sink_stats->set_self_execution_time_ns(5000);
    grpc::ClientContext done_context;
    auto s = stub_->Done(&done_context, done_req, &done_resp);
    EXPECT_TRUE(s.ok());
  }

  EXPECT_EQ(1, source_node.row_batches.size());
  EXPECT_EQ(
      1,
      source_node.row_batches.at(0)->row_batch_result().row_batch().cols(0).int64_data().data(0));
  EXPECT_EQ(1, num_continues);

  uuidpb::UUID agent_uuid_pb;
  ToProto(agent_uuid, &agent_uuid_pb);
  auto exec_stats_or_s = service_->GetIncomingWorkerExecStats(query_uuid, {agent_uuid_pb});

  auto exec_stats = exec_stats_or_s.ConsumeValueOrDie();
  EXPECT_EQ(exec_stats.size(), 1);
  LOG(INFO) << exec_stats[0].DebugString();
}

TEST_F(GRPCRouterTest, delete_node_router_test) {
  int64_t grpc_source_node_id = 1;
  ResetStub();
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::BOOLEAN});
  auto query_uuid = sole::rebuild("ea8aa095-697f-49f1-b127-d50e5b6e2645");

  auto op_proto = planpb::testutils::CreateTestGRPCSource1PB();
  std::unique_ptr<pl::carnot::plan::Operator> plan_node =
      plan::GRPCSourceOperator::FromProto(op_proto, grpc_source_node_id);
  auto source_node = FakeGRPCSourceNode();
  ASSERT_OK(source_node.Init(*plan_node, input_rd, {}));

  auto s = service_->AddGRPCSourceNode(query_uuid, grpc_source_node_id, &source_node, [] {});
  ASSERT_OK(s);

  service_->DeleteQuery(query_uuid);
}

TEST_F(GRPCRouterTest, done_query_test) {
  int64_t grpc_source_node_id = 1;
  ResetStub();
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::BOOLEAN});
  auto query_uuid = sole::rebuild("ea8aa095-697f-49f1-b127-d50e5b6e2645");

  auto op_proto = planpb::testutils::CreateTestGRPCSource1PB();
  std::unique_ptr<pl::carnot::plan::Operator> plan_node =
      plan::GRPCSourceOperator::FromProto(op_proto, grpc_source_node_id);
  auto source_node = FakeGRPCSourceNode();
  ASSERT_OK(source_node.Init(*plan_node, input_rd, {}));

  auto s = service_->AddGRPCSourceNode(query_uuid, grpc_source_node_id, &source_node, [] {});
  ASSERT_OK(s);

  service_->DeleteQuery(query_uuid);
}

// This test is a TSAN test. IT should be run enough times so that all possible
// race conditions will be met.
TEST_F(GRPCRouterTest, threaded_router_test) {
  ResetStub();
  auto query_id_str = "ea8aa095-697f-49f1-b127-d50e5b6e2645";
  auto query_uuid = sole::rebuild(query_id_str);

  auto func_registry_ = std::make_unique<udf::Registry>("test_registry");
  auto table_store = std::make_shared<table_store::TableStore>();
  auto exec_state = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                                MockResultSinkStubGenerator, sole::uuid4());

  MockExecNode mock_child;

  RowDescriptor input_rd({types::DataType::INT64});
  auto op_proto = planpb::testutils::CreateTestGRPCSource1PB();
  std::unique_ptr<pl::carnot::plan::Operator> plan_node =
      plan::GRPCSourceOperator::FromProto(op_proto, 1);
  auto source_node = GRPCSourceNode();
  ASSERT_OK(source_node.Init(*plan_node, input_rd, {}));
  source_node.AddChild(&mock_child, 0);
  ASSERT_OK(source_node.Open(exec_state.get()));
  ASSERT_OK(source_node.Prepare(exec_state.get()));

  FakePlanNode fake_plan_node(111);
  // Silence GMOCK warnings.
  EXPECT_CALL(mock_child, InitImpl(::testing::_));
  EXPECT_CALL(mock_child, PrepareImpl(::testing::_));
  EXPECT_CALL(mock_child, OpenImpl(::testing::_));
  ASSERT_OK(mock_child.Init(fake_plan_node, RowDescriptor({}), {}));
  ASSERT_OK(mock_child.Open(exec_state.get()));
  ASSERT_OK(mock_child.Prepare(exec_state.get()));

  pl::carnotpb::TransferResultChunkResponse response;
  grpc::ClientContext context;
  auto writer = stub_->TransferResultChunk(&context, &response);

  // Start up thread that enqueues row batches.
  std::thread write_thread([&] {
    for (int idx = 0; idx <= 100; ++idx) {
      auto rb = RowBatchBuilder(input_rd, /*size*/ 1, /*eow*/ idx == 100, /*eos*/ idx == 100)
                    .AddColumn<types::Int64Value>({
                        idx,
                    })
                    .get();
      carnotpb::TransferResultChunkRequest rb_req;
      EXPECT_OK(rb.ToProto(rb_req.mutable_row_batch_result()->mutable_row_batch()));
      rb_req.set_address(hostname);
      rb_req.mutable_row_batch_result()->set_grpc_source_id(0);
      auto query_id = rb_req.mutable_query_id();
      query_id->set_data(query_id_str);
      writer->Write(rb_req);
    }
    writer->WritesDone();
    writer->Finish();
  });
  write_thread.join();

  // Start up thread which adds the source node to the router and verifies rowbatches have been
  // enqueued in correct order.
  std::thread read_thread([&] {
    auto idx = 0;

    auto s = service_->AddGRPCSourceNode(query_uuid, /* source_id */ 0, &source_node, [] {});
    ASSERT_OK(s);
    do {
      auto check_result_batch = [&](ExecState*, const table_store::schema::RowBatch& rb, int64_t) {
        EXPECT_EQ(idx,
                  types::GetValueFromArrowArray<types::DataType::INT64>(rb.ColumnAt(0).get(), 0));
      };

      EXPECT_CALL(mock_child, ConsumeNextImpl(::testing::_, ::testing::_, ::testing::_))
          .Times(1)
          .WillRepeatedly(::testing::DoAll(::testing::Invoke(check_result_batch),
                                           ::testing::Return(Status::OK())))
          .RetiresOnSaturation();

      ASSERT_OK(source_node.GenerateNext(exec_state.get()));
      idx++;
    } while (source_node.HasBatchesRemaining());
  });
  // Sleep for 1ms to allow some row batches to enqueue before the source node is added.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  read_thread.join();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
