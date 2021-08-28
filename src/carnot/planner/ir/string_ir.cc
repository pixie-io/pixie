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

#include <pypa/ast/ast.hh>

#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/ir/string_ir.h"

namespace px {
namespace carnot {
namespace planner {

Status StringIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_string_value(str_);
  return Status::OK();
}

uint64_t StringIR::HashValueImpl() const { return ::util::Hash64(str_); }

std::string StringIR::DebugString() const {
  return absl::Substitute("$0(id=$1, val=$2)", type_string(), id(), str());
}

Status StringIR::Init(std::string str) {
  str_ = str;
  return Status::OK();
}

Status StringIR::CopyFromNodeImpl(const IRNode* source,
                                  absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const StringIR* input = static_cast<const StringIR*>(source);
  str_ = input->str_;
  return Status::OK();
}

bool StringIR::NodeMatches(IRNode* node) { return Match(node, String()); }
}  // namespace planner
}  // namespace carnot
}  // namespace px
