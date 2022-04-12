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

#include "src/carnot/exec/map_node.h"

#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using types::Int64Value;
using udf::FunctionContext;

// TOOD(zasgar): refactor these into test udfs.
class AddUDF : public udf::ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext*, types::Int64Value v1, types::Int64Value v2) {
    return v1.val + v2.val;
  }
};

class MapNodeTest : public ::testing::Test {
 public:
  MapNodeTest() {
    auto op_proto = planpb::testutils::CreateTestMapAddTwoCols();
    plan_node_ = plan::MapOperator::FromProto(op_proto, 1);

    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    EXPECT_OK(func_registry_->Register<AddUDF>("add"));
    auto table_store = std::make_shared<table_store::TableStore>();

    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);
    EXPECT_OK(exec_state_->AddScalarUDF(
        0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::INT64})));
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

TEST_F(MapNodeTest, basic) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<MapNode, plan::MapOperator>(*plan_node_, output_rd, {},
                                                                 exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, false, false)
                          .AddColumn<types::Int64Value>({2, 5, 9, 13})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .get(),
                   0)
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 3, true, true).AddColumn<types::Int64Value>({2, 6, 9}).get())
      .Close();
}

TEST_F(MapNodeTest, zero_row_row_batch) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<MapNode, plan::MapOperator>(*plan_node_, output_rd, {},
                                                                 exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, false, false)
                          .AddColumn<types::Int64Value>({2, 5, 9, 13})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 0, true, true)
                       .AddColumn<types::Int64Value>({})
                       .AddColumn<types::Int64Value>({})
                       .get(),
                   0)
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 0, true, true).AddColumn<types::Int64Value>({}).get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .get(),
                   0)
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 3, false, false).AddColumn<types::Int64Value>({2, 6, 9}).get())
      .Close();
}

TEST_F(MapNodeTest, child_fail) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<MapNode, plan::MapOperator>(*plan_node_, output_rd, {},
                                                                 exec_state_.get());
  tester.ConsumeNextShouldFail(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                                   .AddColumn<types::Int64Value>({1, 2, 3, 4})
                                   .AddColumn<types::Int64Value>({1, 3, 6, 9})
                                   .get(),
                               0, error::InvalidArgument("args"));
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
