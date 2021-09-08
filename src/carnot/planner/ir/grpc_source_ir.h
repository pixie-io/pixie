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
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief This is the GRPC Source group that is what will appear in the physical plan.
 * Each GRPCSourceGroupIR will end up being converted into a set of GRPCSourceIR for the
 * corresponding remote_source_ids.
 */
class GRPCSourceIR : public OperatorIR {
 public:
  explicit GRPCSourceIR(int64_t id) : OperatorIR(id, IRNodeType::kGRPCSource) {}
  Status ToProto(planpb::Operator* op_pb) const override;

  /**
   * @brief Special Init that skips around the Operator init function.
   *
   * @param relation
   * @return Status
   */
  Status Init(TypePtr type) { return SetResolvedType(type); }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return error::Unimplemented("Unexpected call to GRPCSourceIR::RequiredInputColumns");
  }

  bool IsSource() const override { return true; }

  // Type is resolved on Init so no need to do anything.
  Status ResolveType(CompilerState*) { return Status::OK(); }

 protected:
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to GRPCSourceIR::PruneOutputColumnsTo.");
  }
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
