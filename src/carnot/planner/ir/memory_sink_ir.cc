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

#include "src/carnot/planner/ir/memory_sink_ir.h"

namespace px {
namespace carnot {
namespace planner {

Status MemorySinkIR::Init(OperatorIR* parent, const std::string& name,
                          const std::vector<std::string> out_columns) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  name_ = name;
  out_columns_ = out_columns;
  return Status::OK();
}

Status MemorySinkIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_mem_sink_op();
  pb->set_name(name_);
  op->set_op_type(planpb::MEMORY_SINK_OPERATOR);

  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    pb->add_column_types(types[i]);
    pb->add_column_names(names[i]);
  }

  if (is_type_resolved()) {
    auto table_type = std::static_pointer_cast<TableType>(resolved_type());
    for (const auto& col_name : names) {
      if (table_type->HasColumn(col_name)) {
        PL_ASSIGN_OR_RETURN(auto col_type, table_type->GetColumnType(col_name));
        if (!col_type->IsValueType()) {
          return error::Internal("Attempting to create MemorySink with a non-columnar type.");
        }
        auto val_type = std::static_pointer_cast<ValueType>(col_type);
        pb->add_column_semantic_types(val_type->semantic_type());
      }
    }
  }

  return Status::OK();
}

Status MemorySinkIR::CopyFromNodeImpl(const IRNode* node,
                                      absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const MemorySinkIR* sink_ir = static_cast<const MemorySinkIR*>(node);
  name_ = sink_ir->name_;
  out_columns_ = sink_ir->out_columns_;
  return Status::OK();
}

Status MemorySinkIR::ResolveType(CompilerState* /* compiler_state */) {
  DCHECK_EQ(1, parent_types().size());
  // When out_columns_ is empty, the MemorySink just copies the parent type.
  if (out_columns_.size() == 0) {
    PL_ASSIGN_OR_RETURN(auto type_ptr, OperatorIR::DefaultResolveType(parent_types()));
    return SetResolvedType(type_ptr);
  }
  auto parent_table_type = std::static_pointer_cast<TableType>(parent_types()[0]);
  auto table = TableType::Create();
  for (const auto& col_name : out_columns_) {
    PL_ASSIGN_OR_RETURN(auto col_type, parent_table_type->GetColumnType(col_name));
    table->AddColumn(col_name, col_type);
  }
  return SetResolvedType(table);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
