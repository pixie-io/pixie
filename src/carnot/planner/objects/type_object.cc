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

#include "src/carnot/planner/objects/type_object.h"
#include "src/carnot/planner/objects/funcobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

Status TypeObject::Init() {
  // Only support casts for Expression Objects.
  if (ql_object_type_ != QLObjectType::kExpr) {
    return Status::OK();
  }

  auto func_name = absl::StrJoin(
      std::vector<std::string>({std::string(types::ToString(data_type_)),
                                std::string(types::ToString(semantic_type_)), "cast"}),
      "_");
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> constructor_fn,
                      FuncObject::Create(func_name, {"expr"}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(&CastHandler::Eval, data_type_, semantic_type_,
                                                   std::placeholders::_1, std::placeholders::_2,
                                                   std::placeholders::_3),
                                         ast_visitor()));
  AddCallMethod(constructor_fn);
  return Status::OK();
}

StatusOr<QLObjectPtr> CastHandler::Eval(types::DataType data_type,
                                        types::SemanticType semantic_type, const pypa::AstPtr&,
                                        const ParsedArgs& args, ASTVisitor*) {
  auto expr_ptr = args.GetArg("expr");
  PX_ASSIGN_OR_RETURN(ExpressionIR * expr, GetArgAs<ExpressionIR>(expr_ptr, "expr"));
  auto new_type = ValueType::Create(data_type, semantic_type);
  expr->SetTypeCast(new_type);
  return expr_ptr;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
