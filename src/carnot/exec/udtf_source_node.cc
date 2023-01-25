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

#include <arrow/array/builder_base.h>
#include <arrow/memory_pool.h>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <magic_enum.hpp>

#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/common/memory/object_pool.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

// TODO(zasgar/philkuz): we should put these in the plan.
// The batch size to use for UDTFs by default.
constexpr int kUDTFBatchSize = 1024;

std::string UDTFSourceNode::DebugStringImpl() {
  return absl::Substitute("Exec::UDTFSourceNode<$0>", plan_node_->DebugString());
}

Status UDTFSourceNode::InitImpl(const plan::Operator& plan_node) {
  const auto* source_plan_node = static_cast<const plan::UDTFSourceOperator*>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::UDTFSourceOperator>(*source_plan_node);

  return Status::OK();
}

Status UDTFSourceNode::PrepareImpl(ExecState* exec_state) {
  // Always has more batches to start with.
  has_more_batches_ = true;
  PX_ASSIGN_OR_RETURN(udtf_def_,
                      exec_state->func_registry()->GetUDTFDefinition(plan_node_->name()));
  return Status::OK();
}

Status UDTFSourceNode::OpenImpl(ExecState* exec_state) {
  function_ctx_ = exec_state->CreateFunctionContext();
  udtf_inst_ = udtf_def_->Make();

  ObjectPool init_args_pool{"udtf_init_args_pool"};
  std::vector<const types::BaseValueType*> init_args;

  for (const auto& sv : plan_node_->init_arguments()) {
    switch (sv.DataType()) {
      case types::BOOLEAN:
        init_args.emplace_back(init_args_pool.Add(
            new types::DataTypeTraits<types::BOOLEAN>::value_type(sv.BoolValue())));
        break;
      case types::INT64:
        init_args.emplace_back(init_args_pool.Add(
            new types::DataTypeTraits<types::INT64>::value_type(sv.Int64Value())));
        break;
      case types::FLOAT64:
        init_args.emplace_back(init_args_pool.Add(
            new types::DataTypeTraits<types::FLOAT64>::value_type(sv.Float64Value())));
        break;
        // TODO(zasgar): Add in int128.
      case types::STRING:
        init_args.emplace_back(init_args_pool.Add(
            new types::DataTypeTraits<types::STRING>::value_type(sv.StringValue())));
        break;
      default:
        CHECK(0) << "Unknown datatype: " << magic_enum::enum_name(sv.DataType());
    }
  }

  return udtf_def_->ExecInit(udtf_inst_.get(), function_ctx_.get(), init_args);
}

Status UDTFSourceNode::CloseImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status UDTFSourceNode::GenerateNextImpl(ExecState* exec_state) {
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> outputs;

  for (const auto& r : udtf_def_->output_relation()) {
    outputs.emplace_back(types::MakeArrowBuilder(r.type(), arrow::default_memory_pool()));
  }

  // TODO(zasgar): Change Exec to take in unique_ptrs.
  std::vector<arrow::ArrayBuilder*> outputs_raw;
  for (auto& out : outputs) {
    outputs_raw.emplace_back(out.get());
  }

  auto has_more_batches = udtf_def_->ExecBatchUpdate(udtf_inst_.get(), function_ctx_.get(),
                                                     kUDTFBatchSize, &outputs_raw);

  DCHECK_GT(outputs.size(), 0U);

  auto rb_or_s = table_store::schema::RowBatch::FromColumnBuilders(
      *output_descriptor_, /*eow*/ !has_more_batches, /*eow*/ !has_more_batches, &outputs);
  if (!rb_or_s.ok()) {
    return rb_or_s.status();
  }
  auto rb = rb_or_s.ConsumeValueOrDie();

  return SendRowBatchToChildren(exec_state, *rb);
}

bool UDTFSourceNode::NextBatchReady() { return HasBatchesRemaining(); }

}  // namespace exec
}  // namespace carnot
}  // namespace px
