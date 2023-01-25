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

#include "src/carnot/planner/ir/udtf_source_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {

Status UDTFSourceIR::Init(std::string_view func_name,
                          const absl::flat_hash_map<std::string, ExpressionIR*>& arg_value_map,
                          const udfspb::UDTFSourceSpec& udtf_spec) {
  func_name_ = func_name;
  udtf_spec_ = udtf_spec;
  table_store::schema::Relation relation;
  PX_RETURN_IF_ERROR(relation.FromProto(&udtf_spec_.relation()));
  PX_RETURN_IF_ERROR(SetResolvedType(TableType::Create(relation)));
  return InitArgValues(arg_value_map, udtf_spec);
}

Status UDTFSourceIR::ToProto(planpb::Operator* op) const {
  op->set_op_type(planpb::UDTF_SOURCE_OPERATOR);

  auto pb = op->mutable_udtf_source_op();
  pb->set_name(func_name_);

  for (const auto& arg_value : arg_values_) {
    PX_RETURN_IF_ERROR(arg_value->ToProto(pb->add_arg_values()));
  }

  return Status::OK();
}

Status UDTFSourceIR::SetArgValues(const std::vector<ExpressionIR*>& arg_values) {
  arg_values_.resize(arg_values.size());
  for (const auto& [idx, value] : Enumerate(arg_values)) {
    if (!value->IsData()) {
      return CreateIRNodeError("expected scalar value, received '$0'", value->type_string());
    }
    PX_ASSIGN_OR_RETURN(arg_values_[idx],
                        graph()->OptionallyCloneWithEdge(this, static_cast<DataIR*>(value)));
  }
  return Status::OK();
}

Status UDTFSourceIR::InitArgValues(
    const absl::flat_hash_map<std::string, ExpressionIR*>& arg_value_map,
    const udfspb::UDTFSourceSpec& udtf_spec) {
  std::vector<ExpressionIR*> arg_values;
  arg_values.reserve(udtf_spec.args().size());

  // This gets the arguments in the order of the UDTF spec.
  for (const auto& arg_spec : udtf_spec.args()) {
    auto arg_value_map_iter = arg_value_map.find(arg_spec.name());
    DCHECK(arg_value_map_iter != arg_value_map.end());
    ExpressionIR* arg = arg_value_map_iter->second;
    arg_values.push_back(arg);
  }
  return SetArgValues(arg_values);
}

Status UDTFSourceIR::CopyFromNodeImpl(
    const IRNode* source, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const UDTFSourceIR* udtf = static_cast<const UDTFSourceIR*>(source);
  func_name_ = udtf->func_name_;
  udtf_spec_ = udtf->udtf_spec_;
  std::vector<ExpressionIR*> arg_values;
  for (const DataIR* arg_element : udtf->arg_values_) {
    PX_ASSIGN_OR_RETURN(IRNode * new_arg_element, graph()->CopyNode(arg_element, copied_nodes_map));
    DCHECK(Match(new_arg_element, DataNode()));
    arg_values.push_back(static_cast<DataIR*>(new_arg_element));
  }
  return SetArgValues(arg_values);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
