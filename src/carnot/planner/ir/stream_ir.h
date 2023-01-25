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

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

// StreamIR Is a logical node that marks that a DataFrame should be executed in
// a streaming fashion (with continuous reads from its MemorySource) rather than batch.
class StreamIR : public OperatorIR {
 public:
  StreamIR() = delete;
  explicit StreamIR(int64_t id) : OperatorIR(id, IRNodeType::kStream) {}
  Status Init(OperatorIR* parent) {
    PX_RETURN_IF_ERROR(AddParent(parent));
    return Status::OK();
  }

  Status ToProto(planpb::Operator*) const override {
    return error::Unimplemented("StreamIR::ToProto should not be called");
  }
  Status CopyFromNodeImpl(const IRNode*, absl::flat_hash_map<const IRNode*, IRNode*>*) override {
    return Status::OK();
  }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return error::Unimplemented("StreamIR::RequiredInputColumns should not be called");
  }
  static constexpr bool FailOnResolveType() { return true; }

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>&) override {
    return error::Unimplemented("StreamIR::PruneOutputColumnsToImpl should not be called");
  }
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
