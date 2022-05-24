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

#include "src/carnot/exec/grpc_sink_node.h"

#include <utility>
#include <vector>

#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/carnotpb/carnot.pb.h"
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
using ::testing::Invoke;
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
        MockMetricsStubGenerator, MockTraceStubGenerator, sole::uuid4(), nullptr, nullptr,
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
constexpr char kExpected0RowResult[] = R"proto(
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
      absl::Substitute(kExpected0RowResult, exec_state_->query_id().ab, exec_state_->query_id().cd),
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

constexpr char kExpectedExternal0RowResult[] = R"proto(
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
      absl::Substitute(kExpectedExternal0RowResult, exec_state_->query_id().ab,
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

  EXPECT_CALL(*writer, WritesDone()).WillOnce(Return(true));
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));

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

struct SplitTestCase {
  size_t max_batch_size = 4096;
  float batch_size_factor = 0.5;
  int64_t num_rows;
  size_t num_int64_cols;
  size_t num_uint128_cols;
  std::vector<std::vector<types::StringValue>> string_cols;
  std::vector<int64_t> expected_num_rows_per_batch;
  bool eos;
  bool eow;
};

class GRPCSinkNodeSplitTest : public GRPCSinkNodeTest,
                              public ::testing::WithParamInterface<SplitTestCase> {};

TEST_P(GRPCSinkNodeSplitTest, break_up_batches) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink1PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());

  auto test_case = GetParam();

  std::vector<types::DataType> types;
  for (size_t i = 0; i < test_case.num_int64_cols; ++i) {
    types.push_back(types::DataType::INT64);
  }
  for (size_t i = 0; i < test_case.num_uint128_cols; ++i) {
    types.push_back(types::DataType::UINT128);
  }
  for (size_t i = 0; i < test_case.string_cols.size(); ++i) {
    types.push_back(types::DataType::STRING);
  }
  RowDescriptor input_rd(types);
  RowDescriptor output_rd(types);

  google::protobuf::util::MessageDifferencer differ;

  TransferResultChunkResponse resp;
  resp.set_success(true);

  auto num_output_batches = test_case.expected_num_rows_per_batch.size();
  std::vector<TransferResultChunkRequest> actual_protos;

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  auto save_arg = [&](TransferResultChunkRequest req, grpc::WriteOptions) {
    actual_protos.push_back(req);
  };
  EXPECT_CALL(*writer, Write(_, _))
      .Times(num_output_batches + 1)
      .WillOnce(Return(true))
      .WillRepeatedly(DoAll(Invoke(save_arg), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get(), test_case.max_batch_size,
      test_case.batch_size_factor);

  auto row_batch_builder =
      RowBatchBuilder(output_rd, test_case.num_rows, test_case.eow, test_case.eos);
  std::vector<types::Int64Value> int64_data(test_case.num_rows, 1);
  std::vector<types::UInt128Value> uint128_data(test_case.num_rows, absl::uint128());
  for (size_t i = 0; i < test_case.num_int64_cols; ++i) {
    row_batch_builder.AddColumn<types::Int64Value>(int64_data);
  }
  for (size_t i = 0; i < test_case.num_uint128_cols; ++i) {
    row_batch_builder.AddColumn<types::UInt128Value>(uint128_data);
  }
  for (const auto& string_col : test_case.string_cols) {
    row_batch_builder.AddColumn<types::StringValue>(string_col);
  }
  auto rb = row_batch_builder.get();
  tester.ConsumeNext(rb, 5, 0);

  for (const auto& [idx, expected_num_rows] : Enumerate(test_case.expected_num_rows_per_batch)) {
    EXPECT_EQ(actual_protos[idx].query_result().row_batch().num_rows(), expected_num_rows);
    if (idx != num_output_batches - 1) {
      EXPECT_EQ(actual_protos[idx].query_result().row_batch().eow(), false);
      EXPECT_EQ(actual_protos[idx].query_result().row_batch().eos(), false);
    } else {
      EXPECT_EQ(actual_protos[idx].query_result().row_batch().eow(), test_case.eow);
      EXPECT_EQ(actual_protos[idx].query_result().row_batch().eos(), test_case.eos);
    }
  }

  tester.Close();
}

const SplitTestCase split_test_cases[] = {
    // No split case
    SplitTestCase{
        .num_rows = 10,
        .num_int64_cols = 2,
        .num_uint128_cols = 2,
        .string_cols = {},
        .expected_num_rows_per_batch = std::vector<int64_t>{10},
        .eos = true,
        .eow = true,
    },
    // Simple split without strings.
    SplitTestCase{
        .num_rows = 49,
        .num_int64_cols = 8,
        .num_uint128_cols = 4,
        .string_cols = {},
        .expected_num_rows_per_batch = std::vector<int64_t>{16, 16, 16, 1},
        .eos = true,
        .eow = true,
    },
    // Split without strings, no eos.
    SplitTestCase{
        .num_rows = 49,
        .num_int64_cols = 8,
        .num_uint128_cols = 4,
        .string_cols = {},
        .expected_num_rows_per_batch = std::vector<int64_t>{16, 16, 16, 1},
        .eos = false,
        .eow = false,
    },
    // Split with just strings.
    SplitTestCase{
        .max_batch_size = 16,
        .batch_size_factor = 1.0f,
        .num_rows = 7,
        .num_int64_cols = 0,
        .num_uint128_cols = 0,
        .string_cols =
            std::vector<std::vector<types::StringValue>>{
                std::vector<types::StringValue>(7, "abcd"),
                std::vector<types::StringValue>(7, "12"),
            },
        // Each row is 6 bytes, so only 2 rows will fit in the 16 byte max batch size.
        .expected_num_rows_per_batch = std::vector<int64_t>{2, 2, 2, 1},
        .eos = true,
        .eow = true,
    },
    // Split with heterogeneous strings.
    SplitTestCase{
        .max_batch_size = 32,
        .batch_size_factor = 1.0f,
        .num_rows = 6,
        .num_int64_cols = 0,
        .num_uint128_cols = 0,
        .string_cols =
            std::vector<std::vector<types::StringValue>>{
                std::vector<types::StringValue>{
                    // This first row should be its own batch.
                    std::string(32, 'X'),
                    // These 3 rows should be one batch.
                    "1",
                    "2",
                    "3",
                    // These last 2 should end up in a batch together.
                    std::string(31, 'X'),
                    "4",
                },
            },
        .expected_num_rows_per_batch = std::vector<int64_t>{1, 3, 2},
        .eos = true,
        .eow = true,
    },
    // Split with strings and other cols.
    SplitTestCase{
        // There are 38 bytes per row, we test setting the max to just short of 4 rows.
        .max_batch_size = 38 + 38 + 38 + 37,
        .batch_size_factor = 1.0f,
        .num_rows = 12,
        .num_int64_cols = 2,
        .num_uint128_cols = 1,
        .string_cols =
            std::vector<std::vector<types::StringValue>>{
                std::vector<types::StringValue>(12, "abcd"),
                std::vector<types::StringValue>(12, "12"),
            },
        .expected_num_rows_per_batch = std::vector<int64_t>{3, 3, 3, 3},
        .eos = false,
        .eow = false,
    },
    // Split with string row larger than max batch size. For now, the best we can do is just return
    // it as a batch on its own.
    SplitTestCase{
        .max_batch_size = 32,
        .batch_size_factor = 1.0f,
        .num_rows = 6,
        .num_int64_cols = 0,
        .num_uint128_cols = 0,
        .string_cols =
            std::vector<std::vector<types::StringValue>>{
                std::vector<types::StringValue>{
                    "abcd",
                    "1234",
                    std::string(100, 'X'),
                    std::string(32, 'X'),
                    "1",
                    std::string(31, 'X'),
                },
            },
        .expected_num_rows_per_batch = std::vector<int64_t>{2, 1, 1, 2},
        .eos = true,
        .eow = true,
    },
    // Split with non-string rows larger than max batch size. For now, the best we can do is just
    // return it as a batch on its own.
    SplitTestCase{
        .max_batch_size = 32,
        .batch_size_factor = 1.0f,
        .num_rows = 6,
        .num_int64_cols = 6,
        .num_uint128_cols = 0,
        .string_cols = {},
        .expected_num_rows_per_batch = std::vector<int64_t>{1, 1, 1, 1, 1, 1},
        .eos = true,
        .eow = true,
    },
};

INSTANTIATE_TEST_SUITE_P(SplitBatchesTest, GRPCSinkNodeSplitTest,
                         ::testing::ValuesIn(split_test_cases));

TEST_F(GRPCSinkNodeTest, retry_failed_writes) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink1PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  TransferResultChunkResponse resp;
  resp.set_success(true);

  std::vector<TransferResultChunkRequest> actual_protos(5);
  std::vector<std::string> expected_protos = {
      absl::Substitute(kExpected0RowResult, exec_state_->query_id().ab, exec_state_->query_id().cd),
      absl::Substitute(kExpectedInteralResult0, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
      absl::Substitute(kExpectedInteralResult1, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
      absl::Substitute(kExpected0RowResult, exec_state_->query_id().ab, exec_state_->query_id().cd),
      absl::Substitute(kExpectedInteralResult2, exec_state_->query_id().ab,
                       exec_state_->query_id().cd),
  };

  auto writer1 = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();
  auto writer2 = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  EXPECT_CALL(*writer1, Write(_, _))
      .Times(4)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)))
      .WillOnce(Return(false));

  EXPECT_CALL(*writer2, Write(_, _))
      .Times(2)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[3]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[4]), Return(true)));

  EXPECT_CALL(*writer1, WritesDone()).WillOnce(Return(true));
  EXPECT_CALL(*writer2, WritesDone()).WillOnce(Return(true));

  EXPECT_CALL(*writer1, Finish())
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::INTERNAL, "Received RST_STREAM with code 2")));
  EXPECT_CALL(*writer2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .Times(2)
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer1)))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer2)));

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

  for (auto i = 0; i < 5; ++i) {
    EXPECT_THAT(actual_protos[i], EqualsProto(expected_protos[i]));
  }

  EXPECT_FALSE(add_metadata_called_);
}

TEST_F(GRPCSinkNodeTest, check_connection_after_eos) {
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
      .Times(2)
      .WillOnce(Return(true))   // Initiate result sink
      .WillOnce(Return(true));  // Successful batch with eos.

  // If WritesDone is called more than once, it will cause a segfault.
  EXPECT_CALL(*writer, WritesDone()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  tester.node()->testing_set_connection_check_timeout(std::chrono::milliseconds(-1));

  std::vector<types::Int64Value> data(1, 1);
  auto rb = RowBatchBuilder(output_rd, 1, /*eow*/ true, /*eos*/ true)
                .AddColumn<types::Int64Value>(data)
                .get();
  tester.ConsumeNext(rb, 5, 0);

  // The check should just return status ok after an eos is sent.
  EXPECT_OK(tester.node()->OptionallyCheckConnection(exec_state_.get()));

  tester.Close();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
