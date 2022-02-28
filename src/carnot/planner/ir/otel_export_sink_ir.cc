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

#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/otel_export_sink_ir.h"
#include "src/carnot/planpb/plan.pb.h"

namespace px {
namespace carnot {
namespace planner {

Status OTelExportSinkIR::Init(OperatorIR* parent, const planpb::OTelExportSinkOperator& otel_op,
                              const std::vector<ExpectedColumn>& expected_columns) {
  otel_op_ = otel_op;
  expected_columns_ = expected_columns;
  return AddParent(parent);
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> OTelExportSinkIR::RequiredInputColumns()
    const {
  std::vector<absl::flat_hash_set<std::string>> required_sets;
  auto& required = required_sets.emplace_back();
  for (const auto& col : expected_columns_) {
    required.insert(col.colname);
  }
  return required_sets;
}

Status OTelExportSinkIR::ToProto(planpb::Operator* op) const {
  *op->mutable_otel_sink_op() = otel_op_;

  op->set_op_type(planpb::OTEL_EXPORT_SINK_OPERATOR);
  return Status::OK();
}

Status OTelExportSinkIR::CopyFromNodeImpl(const IRNode* node,
                                          absl::flat_hash_map<const IRNode*, IRNode*>*) {
  auto source = static_cast<const OTelExportSinkIR*>(node);
  otel_op_ = source->otel_op_;
  return Status::OK();
}

Status OTelExportSinkIR::UpdateOpAfterParentTypesResolvedImpl() {
  DCHECK_EQ(1UL, parents().size());
  auto table_type = parents()[0]->resolved_table_type();

  for (const auto& col : expected_columns_) {
    auto col_type_or_s = table_type->GetColumnType(col.colname);
    if (!col_type_or_s.ok()) {
      return col.error_node->CreateIRNodeError("Column '$0' not found in parent dataframe",
                                               col.colname);
    }
    auto col_type = std::static_pointer_cast<ValueType>(col_type_or_s.ConsumeValueOrDie());
    if (!col.coltypes.contains(col_type->data_type())) {
      std::string type_message;
      if (col.coltypes.size() == 1) {
        type_message = absl::Substitute("a $0", types::ToString(*col.coltypes.begin()));
      } else {
        type_message = absl::Substitute(
            "one of $0", absl::StrJoin(col.coltypes, ",", [](std::string* out, types::DataType dt) {
              absl::StrAppend(out, types::ToString(dt));
            }));
      }
      return col.error_node->CreateIRNodeError("Expected '$0' column to be $1, '$2' is of type $3",
                                               col.arg, type_message, col.colname,
                                               types::ToString(col_type->data_type()));
    }
  }
  return Status::OK();
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
