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
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/grpc_sink_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief This is an IR only node that is used to mark where a GRPC source should go.
 * For the actual plan, this operator is replaced with a series of GRPCSourceOperators that are
 * Unioned together.
 */
class GRPCSourceGroupIR : public OperatorIR {
 public:
  explicit GRPCSourceGroupIR(int64_t id) : OperatorIR(id, IRNodeType::kGRPCSourceGroup) {}

  Status ToProto(planpb::Operator* op_pb) const override;

  /**
   * @brief Special Init that skips around the Operator init function.
   *
   * @param source_id
   * @param relation
   * @return Status
   */
  Status Init(int64_t source_id, TypePtr type) {
    source_id_ = source_id;
    return SetResolvedType(type);
  }

  void SetGRPCAddress(const std::string& grpc_address) { grpc_address_ = grpc_address; }
  void SetSSLTargetName(const std::string& ssl_targetname) { ssl_targetname_ = ssl_targetname; }

  /**
   * @brief Associate the passed in GRPCSinkOperator with this Source Group. The sink_op passed in
   * will most likely exist outside of the graph this contains, so instead of holding a pointer, we
   * hold the information that will be used during execution in Vizier.
   *
   * @param sink_op: the sink operator that should be connected with this source operator.
   * @return Status: error if this->source_id and sink_op->destination_id don't line up.
   */
  Status AddGRPCSink(GRPCSinkIR* sink_op, const absl::flat_hash_set<int64_t>& agents);
  bool GRPCAddressSet() const { return grpc_address_ != ""; }
  const std::string& grpc_address() const { return grpc_address_; }
  int64_t source_id() const { return source_id_; }
  const std::vector<std::pair<GRPCSinkIR*, absl::flat_hash_set<int64_t>>>& dependent_sinks() {
    return dependent_sinks_;
  }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return error::Unimplemented("Unexpected call to GRPCSourceGroupIR::RequiredInputColumns");
  }

  bool IsSource() const override { return true; }

  static constexpr bool FailOnResolveType() { return true; }

 protected:
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to GRPCSourceGroupIR::PruneOutputColumnsTo.");
  }

 private:
  int64_t source_id_ = -1;
  std::string grpc_address_ = "";
  std::string ssl_targetname_ = "";
  std::vector<std::pair<GRPCSinkIR*, absl::flat_hash_set<int64_t>>> dependent_sinks_;
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
