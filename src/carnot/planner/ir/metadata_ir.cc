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

#include "src/carnot/planner/ir/metadata_ir.h"

namespace px {
namespace carnot {
namespace planner {

std::string MetadataIR::DebugString() const {
  return absl::Substitute("$0(id=$1, name=$2)", type_string(), id(), name());
}

/* Metadata IR */
Status MetadataIR::Init(const std::string& metadata_str, int64_t parent_op_idx) {
  // Note, metadata_str is a temporary name. It is updated in ResolveMetadataColumn.
  PX_RETURN_IF_ERROR(ColumnIR::Init(metadata_str, parent_op_idx));
  metadata_name_ = metadata_str;
  return Status::OK();
}

Status MetadataIR::CopyFromNodeImpl(const IRNode* node,
                                    absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const MetadataIR* metadata_ir = static_cast<const MetadataIR*>(node);
  metadata_name_ = metadata_ir->metadata_name_;
  property_ = metadata_ir->property_;
  return Status::OK();
}

Status MetadataIR::ResolveType(CompilerState*, const std::vector<TypePtr>&) {
  DCHECK(has_property());
  auto type = ValueType::Create(property_->column_type(), types::ST_NONE);
  return SetResolvedType(type);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
