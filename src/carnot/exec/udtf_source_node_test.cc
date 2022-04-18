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

#include "src/carnot/exec/udtf_source_node.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using types::Int64Value;
using types::StringValue;
using udf::ColInfo;
using udf::FunctionContext;
using udf::UDTF;
using udf::UDTFArg;

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

class BasicTestUDTF : public UDTF<BasicTestUDTF> {
 public:
  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg::Make<types::DataType::INT64>("some_int", "Int arg"),
                     UDTFArg::Make<types::DataType::STRING>("some_string", "String arg"));
  }

  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_int", types::DataType::INT64, types::PatternType::GENERAL, "int result"),
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"));
  }

  Status Init(FunctionContext*, Int64Value some_int, StringValue some_string) {
    some_int_ = some_int.val;
    some_string_ = std::string(some_string);
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (idx++ < 2) {
      rw->Append<IndexOf("out_str")>(some_string_ + std::to_string(idx));
      rw->Append<IndexOf("out_int")>(some_int_ + idx);
      return true;
    }
    return false;
  }

 private:
  int idx = 0;
  int64_t some_int_;
  std::string some_string_;
};

constexpr char kUDTFTestPbtxt[] = R"proto(
  op_type: UDTF_SOURCE_OPERATOR
  udtf_source_op {
    name: "test_udtf"
    arg_values {
      data_type: INT64
      int64_value: 321
    }
    arg_values {
      data_type: STRING
      string_value: "ts1"
    }
  }
)proto";

class UDTFSourceNodeTest : public ::testing::Test {
 public:
  UDTFSourceNodeTest() {
    planpb::Operator op_pb;
    EXPECT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFTestPbtxt, &op_pb));
    plan_node_ = plan::UDTFSourceOperator::FromProto(op_pb, 1);

    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    EXPECT_OK(func_registry_->Register<BasicTestUDTF>("test_udtf"));
    auto table_store = std::make_shared<table_store::TableStore>();

    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

TEST_F(UDTFSourceNodeTest, single_output_batch_test) {
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::STRING});
  auto tester = exec::ExecNodeTester<UDTFSourceNode, plan::UDTFSourceOperator>(
      *plan_node_, output_rd, {}, exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Int64Value>({322, 323})
          .AddColumn<types::StringValue>({"ts11", "ts12"})
          .get());
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
