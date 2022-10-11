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

#include "src/carnot/planner/ir/uint128_ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {

Status UInt128IR::ToProtoImpl(planpb::ScalarValue* value) const {
  auto uint128pb = value->mutable_uint128_value();
  uint128pb->set_high(absl::Uint128High64(val_));
  uint128pb->set_low(absl::Uint128Low64(val_));
  return Status::OK();
}

uint64_t UInt128IR::HashValueImpl() const {
  uint64_t high_low[] = {absl::Uint128High64(val_), absl::Uint128Low64(val_)};
  return ::util::Hash64(reinterpret_cast<const char*>(high_low), 2 * sizeof(uint64_t));
}

Status UInt128IR::Init(absl::uint128 val) {
  val_ = val;
  return Status::OK();
}

Status UInt128IR::Init(const std::string& uuid_str) {
  auto upid_or_s = md::UPID::ParseFromUUIDString(uuid_str);
  if (!upid_or_s.ok()) {
    return CreateIRNodeError(upid_or_s.msg());
  }

  return Init(upid_or_s.ConsumeValueOrDie().value());
}

Status UInt128IR::CopyFromNodeImpl(const IRNode* source,
                                   absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const UInt128IR* input = static_cast<const UInt128IR*>(source);
  val_ = input->val_;
  return Status::OK();
}
bool UInt128IR::NodeMatches(IRNode* node) { return Match(node, UInt128()); }

}  // namespace planner
}  // namespace carnot
}  // namespace px
