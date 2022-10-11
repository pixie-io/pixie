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

#include "src/carnot/planner/ir/time_ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {

Status TimeIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_time64_ns_value(val_);
  return Status::OK();
}

uint64_t TimeIR::HashValueImpl() const {
  return ::util::Hash64(reinterpret_cast<const char*>(&val_), sizeof(int64_t));
}

/* Time IR */
Status TimeIR::Init(int64_t val) {
  val_ = val;
  return Status::OK();
}

Status TimeIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const TimeIR* time_ir = static_cast<const TimeIR*>(node);
  val_ = time_ir->val_;
  return Status::OK();
}

bool TimeIR::NodeMatches(IRNode* node) { return Match(node, Time()); }
}  // namespace planner
}  // namespace carnot
}  // namespace px
