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
#include <memory>
#include <string>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief The ExprObject is a QLObject that is a container for an ExpressionIR.
 *
 * The purpose of this is to use the Object model to assemble arbitrary expressions.
 */
class ExprObject : public QLObject {
 public:
  static constexpr TypeDescriptor ExprType = {
      /* name */ "Expression",
      /* type */ QLObjectType::kExpr,
  };

  static StatusOr<std::shared_ptr<ExprObject>> Create(ExpressionIR* expr, ASTVisitor* visitor) {
    return std::shared_ptr<ExprObject>(new ExprObject(expr, visitor));
  }

  ExpressionIR* expr() { return expr_; }

  std::string name() const override {
    if (expr_->type() != IRNodeType::kFunc) {
      return expr_->type_string();
    }
    return "FuncCall";
  }
  static bool IsExprObject(const QLObjectPtr& object) {
    return object->type() == QLObjectType::kExpr;
  }

 protected:
  ExprObject(ExpressionIR* expr, ASTVisitor* visitor)
      : QLObject(ExprType, expr->ast(), visitor), expr_(expr) {}

 private:
  ExpressionIR* expr_;
};

StatusOr<std::string> GetAsString(const QLObjectPtr& obj);

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
