#include "src/carnot/exec/grpc_sink_node.h"

#include <utility>
#include <vector>

#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/carnotpb/carnot_mock.grpc.pb.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/common/uuid/uuid_utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

using carnotpb::MockResultSinkServiceStub;
using carnotpb::ResultSinkService;
using carnotpb::TransferResultChunkRequest;
using carnotpb::TransferResultChunkResponse;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using testing::proto::EqualsProto;

class GRPCSinkNodeTest : public ::testing::Test {
 public:
  GRPCSinkNodeTest() {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();

    mock_unique_ = std::make_unique<MockResultSinkServiceStub>();
    mock_ = mock_unique_.get();

    exec_state_ = std::make_unique<ExecState>(
        func_registry_.get(), table_store,
        [this](const std::string&,
               const std::string&) -> std::unique_ptr<ResultSinkService::StubInterface> {
          return std::move(mock_unique_);
        },
        sole::uuid4(), nullptr, nullptr,
        [this](grpc::ClientContext*) { add_metadata_called_ = true; });

    table_store::schema::Relation rel({types::DataType::BOOLEAN, types::DataType::TIME64NS},
                                      {"col1", "time_"});
  }

 protected:
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
  MockResultSinkServiceStub* mock_;
  bool add_metadata_called_ = false;

 private:
  // Ownership will be transferred to the GRPC node, so access this ptr via `mock_` in the tests.
  std::unique_ptr<MockResultSinkServiceStub> mock_unique_;
};

constexpr char kExpectedInternalInitialization[] = R"proto(
address: "localhost:1234"
query_id {
  high_bits: $0
  low_bits: $1
}
query_result {
  initiate_result_stream: true
  grpc_source_id: 0
}
)proto";

constexpr char kExpectedInteralResult0[] = R"proto(
address: "localhost:1234"
query_id {
  high_bits: $0
  low_bits: $1
}
query_result {
  row_batch {
    cols {
      int64_data {
      }
    }
  }
  grpc_source_id: 0
}
)proto";

constexpr char kExpectedInteralResult1[] = R"proto(
address: "localhost:1234"
query_id {
  high_bits: $0
  low_bits: $1
}
query_result {
  row_batch {
    cols {
      int64_data {
        data: 1
      }
    }
    num_rows: 1
  }
  grpc_source_id: 0
}
)proto";

constexpr char kExpectedInteralResult2[] = R"proto(
address: "localhost:1234"
query_id {
  high_bits: $0
  low_bits: $1
}
query_result {
  row_batch {
    cols {
      int64_data {
        data: 2
        data: 2
      }
    }
    num_rows: 2
    eow: true
    eos: true
  }
  grpc_source_id: 0
}
)proto";

TEST_F(GRPCSinkNodeTest, internal_result) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink1PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  TransferResultChunkResponse resp;
  resp.set_success(true);

  std::vector<TransferResultChunkRequest> actual_protos(4);
  std::vector<std::string> expected_protos = {
      absl::Substitute(kExpectedInternalInitialization, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
      absl::Substitute(kExpectedInteralResult0, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
      absl::Substitute(kExpectedInteralResult1, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
      absl::Substitute(kExpectedInteralResult2, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
  };

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  EXPECT_CALL(*writer, Write(_, _))
      .Times(4)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[3]), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  for (auto i = 0; i < 3; ++i) {
    std::vector<types::Int64Value> data(i, i);
    auto rb = RowBatchBuilder(output_rd, i, /*eow*/ i == 2, /*eos*/ i == 2)
                  .AddColumn<types::Int64Value>(data)
                  .get();
    tester.ConsumeNext(rb, 5, 0);
  }

  tester.Close();

  for (auto i = 0; i < 4; ++i) {
    EXPECT_THAT(actual_protos[i], EqualsProto(expected_protos[i]));
  }

  EXPECT_FALSE(add_metadata_called_);
}

constexpr char kExpectedExternalInitialization[] = R"proto(
address: "localhost:1234"
query_id {
  high_bits: $0
  low_bits: $1
}
query_result {
  initiate_result_stream: true
  table_name: "output_table_name"
}
)proto";

constexpr char kExpectedExteralResult0[] = R"proto(
address: "localhost:1234"
query_id {
  high_bits: $0
  low_bits: $1
}
query_result {
  row_batch {
    cols {
      int64_data {
      }
    }
  }
  table_name: "output_table_name"
}
)proto";

constexpr char kExpectedExteralResult1[] = R"proto(
address: "localhost:1234"
query_id {
  high_bits: $0
  low_bits: $1
}
query_result {
  row_batch {
    cols {
      int64_data {
        data: 1
      }
    }
    num_rows: 1
  }
  table_name: "output_table_name"
}
)proto";

constexpr char kExpectedExteralResult2[] = R"proto(
address: "localhost:1234"
query_id {
  high_bits: $0
  low_bits: $1
}
query_result {
  row_batch {
    cols {
      int64_data {
        data: 2
        data: 2
      }
    }
    num_rows: 2
    eow: true
    eos: true
  }
  table_name: "output_table_name"
}
)proto";

TEST_F(GRPCSinkNodeTest, external_result) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink2PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  TransferResultChunkResponse resp;
  resp.set_success(true);

  std::vector<TransferResultChunkRequest> actual_protos(4);
  std::vector<std::string> expected_protos = {
      absl::Substitute(kExpectedExternalInitialization, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
      absl::Substitute(kExpectedExteralResult0, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
      absl::Substitute(kExpectedExteralResult1, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
      absl::Substitute(kExpectedExteralResult2, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
  };

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  EXPECT_CALL(*writer, Write(_, _))
      .Times(4)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[3]), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  for (auto i = 0; i < 3; ++i) {
    std::vector<types::Int64Value> data(i, i);
    auto rb = RowBatchBuilder(output_rd, i, /*eow*/ i == 2, /*eos*/ i == 2)
                  .AddColumn<types::Int64Value>(data)
                  .get();
    tester.ConsumeNext(rb, 5, 0);
  }

  tester.Close();

  for (auto i = 0; i < 4; ++i) {
    EXPECT_THAT(actual_protos[i], EqualsProto(expected_protos[i]));
  }

  EXPECT_TRUE(add_metadata_called_);
}

TEST_F(GRPCSinkNodeTest, check_connection) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink2PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  TransferResultChunkResponse resp;
  resp.set_success(true);

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();
  EXPECT_CALL(*writer, Write(_, _))
      .Times(3)
      .WillOnce(Return(true))    // Initiate result sink
      .WillOnce(Return(true))    // Successful OptionallyCheckConnection
      .WillOnce(Return(false));  // Failed OptionallyCheckConnection

  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  tester.node()->testing_set_connection_check_timeout(std::chrono::milliseconds(-1));

  auto before_flush_time = tester.node()->testing_last_send_time();
  EXPECT_OK(tester.node()->OptionallyCheckConnection(exec_state_.get()));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_NOT_OK(tester.node()->OptionallyCheckConnection(exec_state_.get()));
  auto after_flush_time = tester.node()->testing_last_send_time();

  EXPECT_GT(after_flush_time, before_flush_time);

  tester.Close();
}

TEST_F(GRPCSinkNodeTest, update_connection_time) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink2PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  TransferResultChunkResponse resp;
  resp.set_success(true);

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();
  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  EXPECT_CALL(*writer, Write(_, _))
      .Times(2)
      .WillOnce(Return(true))   // Initiate stream
      .WillOnce(Return(true));  // ConsumeNext

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  auto before_flush_time = tester.node()->testing_last_send_time();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  auto rb = RowBatchBuilder(output_rd, 3, /*eow*/ false, /*eos*/ false)
                .AddColumn<types::Int64Value>({1, 2, 3})
                .get();

  tester.ConsumeNext(rb, 5, 0);
  auto after_flush_time = tester.node()->testing_last_send_time();

  EXPECT_GT(after_flush_time, before_flush_time);
}

TEST_F(GRPCSinkNodeTest, break_up_batch_no_leftover) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink1PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  TransferResultChunkResponse resp;
  resp.set_success(true);

  std::vector<TransferResultChunkRequest> actual_protos(6);

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  EXPECT_CALL(*writer, Write(_, _))
      .Times(6)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[3]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[4]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[5]), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  int64_t num_rows = 1024 * 1024 * 2;
  std::vector<types::Int64Value> data(num_rows, 1);
  auto rb = RowBatchBuilder(output_rd, num_rows, /*eow*/ true, /*eos*/ true)
                .AddColumn<types::Int64Value>(data)
                .get();
  tester.ConsumeNext(rb, 5, 0);

  int64_t num_main_batches = 4;
  int64_t row_size = num_rows / num_main_batches;
  // i = 0 batch is a init batch. We have num_main_batches + 1 batches. + 1 => the leftover batch.
  for (int64_t i = 1; i < num_main_batches + 1; ++i) {
    EXPECT_EQ(actual_protos[i].query_result().row_batch().num_rows(), row_size);
    EXPECT_EQ(actual_protos[i].query_result().row_batch().eow(), false);
    EXPECT_EQ(actual_protos[i].query_result().row_batch().eos(), false);
  }

  // This last batch should only return 0 rows, but should have eos and eow true.
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().num_rows(),
            num_rows - num_main_batches * row_size);
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().num_rows(), 0);
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().eow(), true);
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().eos(), true);

  tester.Close();
}

TEST_F(GRPCSinkNodeTest, break_up_batch_with_leftover) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink1PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  std::vector<TransferResultChunkRequest> actual_protos(5);

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  EXPECT_CALL(*writer, Write(_, _))
      .Times(5)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[3]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[4]), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));
  TransferResultChunkResponse resp;
  resp.set_success(true);
  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  int64_t num_rows = static_cast<int64_t>(kMaxBatchSize * 1.5) + 8;
  std::vector<types::Int64Value> data(num_rows, 1);
  auto rb = RowBatchBuilder(output_rd, num_rows, /*eow*/ true, /*eos*/ true)
                .AddColumn<types::Int64Value>(data)
                .get();
  tester.ConsumeNext(rb, 5, 0);

  int64_t num_main_batches = num_rows / static_cast<int64_t>(kMaxBatchSize * kBatchSizeFactor);
  int64_t row_size = num_rows / num_main_batches;
  // i = 0 batch is a init batch. We have num_main_batches + 1 batches. + 1 => the leftover batch.
  for (int64_t i = 1; i < num_main_batches + 1; ++i) {
    EXPECT_EQ(actual_protos[i].query_result().row_batch().num_rows(), row_size);
    EXPECT_EQ(actual_protos[i].query_result().row_batch().eow(), false);
    EXPECT_EQ(actual_protos[i].query_result().row_batch().eos(), false);
  }

  // This last batch should only return 0 rows, but should have eos and eow true.
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().num_rows(),
            num_rows - num_main_batches * row_size);
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().num_rows(),
            num_rows % num_main_batches);
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().eow(), true);
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().eos(), true);

  tester.Close();
}

// Test to make sure we don't always output eow and eos on last split up row batch.
TEST_F(GRPCSinkNodeTest, break_up_row_batch_that_isnt_eow_eos) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink1PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  std::vector<TransferResultChunkRequest> actual_protos(5);

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  EXPECT_CALL(*writer, Write(_, _))
      .Times(5)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[3]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[4]), Return(true)));

  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _)).WillOnce(Return(writer));

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  int64_t num_rows = static_cast<int64_t>(kMaxBatchSize * 1.5) + 8;
  std::vector<types::Int64Value> data(num_rows, 1);
  auto rb = RowBatchBuilder(output_rd, num_rows, /*eow*/ false, /*eos*/ false)
                .AddColumn<types::Int64Value>(data)
                .get();
  tester.ConsumeNext(rb, 5, 0);

  int64_t num_main_batches = num_rows / static_cast<int64_t>(kMaxBatchSize * kBatchSizeFactor);
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().eow(), false);
  EXPECT_EQ(actual_protos[num_main_batches + 1].query_result().row_batch().eos(), false);

  tester.Close();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
