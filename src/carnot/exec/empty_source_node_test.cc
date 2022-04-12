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

#include "src/carnot/exec/empty_source_node.h"

#include <arrow/memory_pool.h>
#include <memory>
#include <utility>
#include <vector>

#include <absl/strings/substitute.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
namespace px {
namespace carnot {
namespace exec {

using table_store::Table;
using table_store::schema::RowDescriptor;
using ::testing::_;

class EmptySourceNodeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);
  }

  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

constexpr char kExpectedEmptySrcPb[] = R"(
  op_type: EMPTY_SOURCE_OPERATOR
  empty_source_op {
    column_names: "cpu0"
    column_names: "cpu1"
    column_types: INT64
    column_types: FLOAT64
  }
)";

TEST_F(EmptySourceNodeTest, basic) {
  planpb::Operator op_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedEmptySrcPb, &op_proto))
      << "Failed to parse proto";
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::INT64, types::FLOAT64});

  auto tester = exec::ExecNodeTester<EmptySourceNode, plan::EmptySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 0, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Int64Value>({})
          .AddColumn<types::Float64Value>({})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();
  EXPECT_EQ(0, tester.node()->RowsProcessed());
  EXPECT_EQ(0, tester.node()->BytesProcessed());
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
