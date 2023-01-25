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

#include "src/carnot/planner/objects/viz_object.h"

#include <utility>
#include "src/carnot/planner/objects/expr_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

Status VisualizationObject::Init() {
  // Setup methods.
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> vega_fn,
                      FuncObject::Create(kVegaAttrId, {"vega_spec"}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(&VegaHandler::Eval, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3),
                                         ast_visitor()));

  AddMethod(kVegaAttrId, vega_fn);
  return Status::OK();
}

StatusOr<std::shared_ptr<VisualizationObject>> VisualizationObject::Create(
    ASTVisitor* ast_visitor) {
  auto viz_object = std::shared_ptr<VisualizationObject>(new VisualizationObject(ast_visitor));
  PX_RETURN_IF_ERROR(viz_object->Init());
  return viz_object;
}

StatusOr<QLObjectPtr> VegaHandler::Eval(const pypa::AstPtr& ast, const ParsedArgs& args,
                                        ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(StringIR * vega_spec_ir, GetArgAs<StringIR>(ast, args, "vega_spec"));
  std::string vega_spec = vega_spec_ir->str();
  return FuncObject::Create(VisualizationObject::kVegaAttrId, {"fn"}, {},
                            /* has_variable_len_args */ false,
                            /* has_variable_len_kwargs */ false,
                            std::bind(&VegaHandler::NestedFn, vega_spec, std::placeholders::_1,
                                      std::placeholders::_2, std::placeholders::_3),
                            visitor);
}

StatusOr<QLObjectPtr> VegaHandler::NestedFn(std::string spec, const pypa::AstPtr& ast,
                                            const ParsedArgs& args, ASTVisitor*) {
  auto fn = args.GetArg("fn");
  PX_ASSIGN_OR_RETURN(auto func, GetCallMethod(ast, fn));

  auto vis_spec = std::make_unique<VisSpec>();
  vis_spec->vega_spec = spec;
  PX_RETURN_IF_ERROR(func->AddVisSpec(std::move(vis_spec)));
  return std::static_pointer_cast<QLObject>(func);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
