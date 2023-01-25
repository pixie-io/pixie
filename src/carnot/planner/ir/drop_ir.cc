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

#include "src/carnot/planner/ir/drop_ir.h"

namespace px {
namespace carnot {
namespace planner {

Status DropIR::Init(OperatorIR* parent, const std::vector<std::string>& drop_cols) {
  PX_RETURN_IF_ERROR(AddParent(parent));
  col_names_ = drop_cols;
  return Status::OK();
}

Status DropIR::ToProto(planpb::Operator*) const {
  return error::Unimplemented("$0 does not have a protobuf.", type_string());
}

Status DropIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const DropIR* drop = static_cast<const DropIR*>(node);
  col_names_ = drop->col_names_;
  return Status::OK();
}

Status DropIR::ResolveType(CompilerState* /* compiler_state */) {
  DCHECK_EQ(1U, parent_types().size());
  auto new_table = std::static_pointer_cast<TableType>(parent_types()[0]->Copy());
  for (const auto& col_name : col_names_) {
    new_table->RemoveColumn(col_name);
  }
  return SetResolvedType(new_table);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
