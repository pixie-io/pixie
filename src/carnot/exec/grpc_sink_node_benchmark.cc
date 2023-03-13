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

#include <utility>
#include <vector>

#include <benchmark/benchmark.h>
#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/carnotpb/carnot_mock.grpc.pb.h"
#include "src/carnot/exec/grpc_sink_node.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/common/uuid/uuid_utils.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/types.pb.h"

using px::carnot::exec::MockMetricsStubGenerator;
using px::carnot::exec::MockTraceStubGenerator;
using px::carnotpb::MockResultSinkServiceStub;
using px::carnotpb::ResultSinkService;
using px::carnotpb::TransferResultChunkRequest;
using px::carnotpb::TransferResultChunkResponse;
using px::table_store::schema::RowBatch;
using px::table_store::schema::RowDescriptor;
using px::types::DataType;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
// NOLINTNEXTLINE : runtime/references.
void BM_GRPCSinkNodeSplitting(benchmark::State& state) {
  auto func_registry = std::make_unique<px::carnot::udf::Registry>("test_registry");
  auto table_store = std::make_shared<px::table_store::TableStore>();

  auto mock_unique = std::make_unique<::testing::NiceMock<MockResultSinkServiceStub>>();
  auto mock = mock_unique.get();

  auto exec_state = std::make_unique<px::carnot::exec::ExecState>(
      func_registry.get(), table_store,
      [&](const std::string&, const std::string&)
          -> std::unique_ptr<ResultSinkService::StubInterface> { return std::move(mock_unique); },
      MockMetricsStubGenerator, MockTraceStubGenerator, sole::uuid4(), nullptr, nullptr,
      [&](grpc::ClientContext*) {});
  TransferResultChunkResponse resp;
  resp.set_success(true);
  auto writer =
      new ::testing::NiceMock<grpc::testing::MockClientWriter<TransferResultChunkRequest>>();
  ON_CALL(*writer, Write(_, _)).WillByDefault(Return(true));
  ON_CALL(*writer, WritesDone()).WillByDefault(Return(true));
  ON_CALL(*writer, Finish()).WillByDefault(Return(grpc::Status::OK));
  ON_CALL(*mock, TransferResultChunkRaw(_, _))
      .WillByDefault(DoAll(SetArgPointee<1>(resp), Return(writer)));

  px::carnot::exec::GRPCSinkNode node;
  auto op_proto = px::carnot::planpb::testutils::CreateTestGRPCSink2PB();
  auto plan_node = std::make_unique<px::carnot::plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());

  auto num_rows = 1024;
  auto num_columns = 4;
  auto string_size = 4 * 1024;

  RowDescriptor input_rd(std::vector<DataType>(num_columns, DataType::STRING));
  RowDescriptor output_rd(std::vector<DataType>(num_columns, DataType::STRING));
  PX_CHECK_OK(node.Init(*plan_node, output_rd, {input_rd}));
  PX_CHECK_OK(node.Prepare(exec_state.get()));
  PX_CHECK_OK(node.Open(exec_state.get()));

  std::string big_string(string_size, 'X');
  std::vector<px::types::StringValue> data(num_rows, big_string);
  auto row_batch_builder =
      px::carnot::exec::RowBatchBuilder(output_rd, num_rows, /*eow*/ true, /*eos*/ true);
  for (int i = 0; i < num_columns; ++i) {
    row_batch_builder.AddColumn<px::types::StringValue>(data);
  }
  auto rb = row_batch_builder.get();

  for (auto _ : state) {
    PX_CHECK_OK(node.ConsumeNext(exec_state.get(), rb, 0));
    state.SetBytesProcessed(num_rows * num_columns * string_size);
  }
}

BENCHMARK(BM_GRPCSinkNodeSplitting)->Unit(benchmark::kMillisecond);
