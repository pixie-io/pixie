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

#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class VisualizationObject : public QLObject {
 public:
  static constexpr TypeDescriptor VisualizationTypeDescriptor = {
      /* name */ "vis",
      /* type */ QLObjectType::kVisualization,
  };
  static StatusOr<std::shared_ptr<VisualizationObject>> Create(ASTVisitor* ast_visitor);

  // Constant for the children of the visualization object.
  inline static constexpr char kVegaAttrId[] = "vega";

 protected:
  explicit VisualizationObject(ASTVisitor* ast_visitor)
      : QLObject(VisualizationTypeDescriptor, ast_visitor) {}

  Status Init();
  IR* graph_;
};

class VegaHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> NestedFn(std::string vega_spec, const pypa::AstPtr& ast,
                                        const ParsedArgs& args, ASTVisitor* visitor);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
