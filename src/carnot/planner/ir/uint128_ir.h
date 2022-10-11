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
#include "src/carnot/planner/ir/data_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/ir_node.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class UInt128IR : public DataIR {
 public:
  UInt128IR() = delete;
  UInt128IR(int64_t id, const ExpressionIR::Annotations& annotations)
      : DataIR(id, IRNodeType::kUInt128, annotations) {}
  explicit UInt128IR(int64_t id) : UInt128IR(id, ExpressionIR::Annotations()) {}

  /**
   * @brief Inits the UInt128 from a absl::uint128 value.
   *
   * @param absl::uint128 uint128 to initialize this from.
   * @return Status
   */
  Status Init(absl::uint128 val);

  /**
   * @brief Inits the UInt128 from a UUID string. If the UUID string is not configured correctly,
   * then returns an error.
   *
   * @return Status::OK if uuid_str is formatted correctly or error if not.
   */
  Status Init(const std::string& uuid_str);

  absl::uint128 val() const { return val_; }
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  Status ToProtoImpl(planpb::ScalarValue* value) const override;

  bool Equals(ExpressionIR* expr) const override {
    if (!NodeMatches(expr)) {
      return false;
    }
    auto data = static_cast<UInt128IR*>(expr);
    return data->val() == val();
  }

  static bool NodeMatches(IRNode* input);

 protected:
  uint64_t HashValueImpl() const override;

 private:
  absl::uint128 val_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
