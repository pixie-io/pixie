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

#include "src/carnot/planner/objects/collection_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

Status CollectionObject::Init() {
  std::shared_ptr<FuncObject> subscript_fn(
      new FuncObject(kSubscriptMethodName, {"key"}, {},
                     /* has_variable_len_args */ false,
                     /* has_variable_len_kwargs */ false,
                     std::bind(&CollectionObject::SubscriptHandler, name(), items_,
                               std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                     ast_visitor()));
  AddSubscriptMethod(subscript_fn);
  return Status::OK();
}

StatusOr<QLObjectPtr> CollectionObject::SubscriptHandler(
    const std::string& name, std::shared_ptr<std::vector<QLObjectPtr>> items,
    const pypa::AstPtr& ast, const ParsedArgs& args, ASTVisitor*) {
  QLObjectPtr key = args.GetArg("key");

  if (!ExprObject::IsExprObject(key)) {
    return key->CreateError("$0 indices must be integers, not $1", name, key->name());
  }
  auto expr = static_cast<ExprObject*>(key.get())->expr();
  if (!IntIR::NodeMatches(expr)) {
    return key->CreateError("$0 indices must be integers, not $1", name, expr->type_string());
  }

  auto idx = static_cast<IntIR*>(expr)->val();
  if (idx >= static_cast<int64_t>(items->size())) {
    return CreateAstError(ast, "$0 index out of range", name);
  }

  return (*items)[idx];
}

std::vector<QLObjectPtr> ObjectAsCollection(QLObjectPtr obj) {
  std::vector<QLObjectPtr> items;
  if (CollectionObject::IsCollection(obj)) {
    items = std::static_pointer_cast<CollectionObject>(obj)->items();
  } else {
    items.push_back(obj);
  }
  return items;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
