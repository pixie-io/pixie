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

#include "src/carnot/planner/ir/data_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class BoolIR : public DataIR {
 public:
  BoolIR() = delete;
  BoolIR(int64_t id, const ExpressionIR::Annotations& annotations)
      : DataIR(id, IRNodeType::kBool, annotations) {}
  explicit BoolIR(int64_t id) : BoolIR(id, ExpressionIR::Annotations()) {}

  Status Init(bool val);

  bool val() const { return val_; }
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  Status ToProtoImpl(planpb::ScalarValue* value) const override;
  static bool NodeMatches(IRNode* input);

  bool Equals(ExpressionIR* expr) const override {
    if (!NodeMatches(expr)) {
      return false;
    }
    auto data = static_cast<BoolIR*>(expr);
    return data->val() == val();
  }

 protected:
  uint64_t HashValueImpl() const override;

 private:
  bool val_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
