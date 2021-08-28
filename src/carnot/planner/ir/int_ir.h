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

#include "src/carnot/planner/ir/data_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace planner {

class IntIR : public DataIR {
 public:
  IntIR() = delete;
  IntIR(int64_t id, const ExpressionIR::Annotations& annotations)
      : DataIR(id, IRNodeType::kInt, annotations) {}
  explicit IntIR(int64_t id) : IntIR(id, ExpressionIR::Annotations()) {}

  Status Init(int64_t val);

  int64_t val() const { return val_; }
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  std::string DebugString() const override {
    return absl::Substitute("$0, $1)", DataIR::DebugString(), val());
  }
  Status ToProtoImpl(planpb::ScalarValue* value) const override;
  static bool NodeMatches(IRNode* input);
  static std::string class_type_string() { return TypeString(IRNodeType::kInt); }

  bool Equals(ExpressionIR* expr) const override {
    if (!NodeMatches(expr)) {
      return false;
    }
    auto data = static_cast<IntIR*>(expr);
    return data->val() == val();
  }

 protected:
  uint64_t HashValueImpl() const override;

 private:
  int64_t val_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
