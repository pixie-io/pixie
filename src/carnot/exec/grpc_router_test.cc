#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <utility>

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/grpc_router.h"
#include "src/carnot/exec/grpc_source_node.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/common/base/base.h"

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
    stub_ = carnotpb::KelvinService::NewStub(channel);
  }

  std::string GetServerAddress() { return absl::Substitute("$0:$1", hostname, port_); }

  std::unique_ptr<carnotpb::KelvinService::Stub> stub_;
  std::unique_ptr<Server> server_;
  std::string hostname = "0.0.0.0";
  int port_ = 0;
  std::unique_ptr<GRPCRouter> service_;
};

class FakeGRPCSourceNode : public pl::carnot::exec::GRPCSourceNode {
 public:
  Status EnqueueRowBatch(std::unique_ptr<pl::carnotpb::RowBatchRequest> row_batch) {
    row_batches.emplace_back(std::move(row_batch));

    return Status::OK();
  }

  std::vector<std::unique_ptr<pl::carnotpb::RowBatchRequest>> row_batches;
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
  auto rb_req1 = carnotpb::RowBatchRequest();
  EXPECT_OK(rb1.ToProto(rb_req1.mutable_row_batch()));
  rb_req1.set_address("localhost");
  rb_req1.set_destination_id(grpc_source_node_id);
  auto query_id = rb_req1.mutable_query_id();
  query_id->set_data(query_id_str);

  auto rb2 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({4, 6})
                 .get();
  auto rb_req2 = carnotpb::RowBatchRequest();
  EXPECT_OK(rb2.ToProto(rb_req2.mutable_row_batch()));
  rb_req2.set_address("localhost");
  rb_req2.set_destination_id(grpc_source_node_id);
  query_id = rb_req2.mutable_query_id();
  query_id->set_data(query_id_str);

  // Send row batches to GRPC router.
  ::pl::carnotpb::RowBatchResponse response;
  grpc::ClientContext context;
  auto writer = stub_->TransferRowBatch(&context, &response);
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

  auto s = service_->AddGRPCSourceNode(query_uuid, grpc_source_node_id, &source_node);
  ASSERT_OK(s);
  EXPECT_EQ(2, source_node.row_batches.size());
  EXPECT_EQ(1, source_node.row_batches.at(0)->row_batch().cols(0).int64_data().data(0));
  EXPECT_EQ(4, source_node.row_batches.at(1)->row_batch().cols(0).int64_data().data(0));
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
  auto s = service_->AddGRPCSourceNode(query_uuid, grpc_source_node_id, &source_node);
  ASSERT_OK(s);
  EXPECT_EQ(0, source_node.row_batches.size());

  // Create row batches.
  auto rb1 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({1, 2})
                 .get();
  auto rb_req1 = carnotpb::RowBatchRequest();
  EXPECT_OK(rb1.ToProto(rb_req1.mutable_row_batch()));
  rb_req1.set_address("localhost");
  rb_req1.set_destination_id(grpc_source_node_id);
  auto query_id = rb_req1.mutable_query_id();
  query_id->set_data(query_id_str);

  auto rb2 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({4, 6})
                 .get();
  auto rb_req2 = carnotpb::RowBatchRequest();
  EXPECT_OK(rb2.ToProto(rb_req2.mutable_row_batch()));
  rb_req2.set_address("localhost");
  rb_req2.set_destination_id(grpc_source_node_id);
  query_id = rb_req2.mutable_query_id();
  query_id->set_data(query_id_str);

  // Send row batches to GRPC router.
  ::pl::carnotpb::RowBatchResponse response;
  grpc::ClientContext context;
  auto writer = stub_->TransferRowBatch(&context, &response);
  writer->Write(rb_req1);
  writer->Write(rb_req2);
  writer->WritesDone();
  writer->Finish();

  EXPECT_EQ(2, source_node.row_batches.size());
  EXPECT_EQ(1, source_node.row_batches.at(0)->row_batch().cols(0).int64_data().data(0));
  EXPECT_EQ(4, source_node.row_batches.at(1)->row_batch().cols(0).int64_data().data(0));
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

  auto s = service_->AddGRPCSourceNode(query_uuid, grpc_source_node_id, &source_node);
  ASSERT_OK(s);

  service_->DeleteQuery(query_uuid);
}

// This test is a TSAN test. IT should be run enough times so that all possible
// race conditions will be met.
TEST_F(GRPCRouterTest, threaded_router_test) {
  ResetStub();
  auto query_id_str = "ea8aa095-697f-49f1-b127-d50e5b6e2645";
  auto query_uuid = sole::rebuild(query_id_str);

  auto udf_registry_ = std::make_unique<udf::ScalarUDFRegistry>("test_registry");
  auto uda_registry_ = std::make_unique<udf::UDARegistry>("test_registry");
  auto udtf_registry_ = std::make_unique<udf::UDTFRegistry>("test_registry");
  auto table_store = std::make_shared<table_store::TableStore>();
  auto exec_state =
      std::make_unique<ExecState>(udf_registry_.get(), uda_registry_.get(), udtf_registry_.get(),
                                  table_store, MockKelvinStubGenerator, sole::uuid4());

  MockExecNode mock_child;

  RowDescriptor input_rd({types::DataType::INT64});
  auto op_proto = planpb::testutils::CreateTestGRPCSource1PB();
  std::unique_ptr<pl::carnot::plan::Operator> plan_node =
      plan::GRPCSourceOperator::FromProto(op_proto, 1);
  auto source_node = GRPCSourceNode();
  ASSERT_OK(source_node.Init(*plan_node, input_rd, {}));
  source_node.AddChild(&mock_child, 0);

  ::pl::carnotpb::RowBatchResponse response;
  grpc::ClientContext context;
  auto writer = stub_->TransferRowBatch(&context, &response);

  // Start up thread that enqueues row batches.
  std::thread write_thread([&] {
    for (int idx = 0; idx <= 100; ++idx) {
      auto rb = RowBatchBuilder(input_rd, 1, /*eow*/ idx == 100, /*eos*/ idx == 100)
                    .AddColumn<types::Int64Value>({
                        idx,
                    })
                    .get();
      auto rb_req = carnotpb::RowBatchRequest();
      EXPECT_OK(rb.ToProto(rb_req.mutable_row_batch()));
      rb_req.set_address("localhost");
      rb_req.set_destination_id(0);
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

    auto s = service_->AddGRPCSourceNode(query_uuid, /* source_id */ 0, &source_node);
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
