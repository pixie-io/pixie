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

#include "src/carnot/planner/ir/ast_utils.h"

namespace px {
namespace carnot {
namespace planner {

std::string GetAstTypeName(pypa::AstType type) {
  std::vector<std::string> type_names = {
#undef PYPA_AST_TYPE
#define PYPA_AST_TYPE(X) #X,
// NOLINTNEXTLINE(build/include_order).
#include <pypa/ast/ast_type.inl>
#undef PYPA_AST_TYPE
  };
  DCHECK(type_names.size() > static_cast<size_t>(type));
  return absl::Substitute("$0", type_names[static_cast<int>(type)]);
}

std::string GetNameAsString(const pypa::AstPtr& node) { return PYPA_PTR_CAST(Name, node)->id; }

StatusOr<std::string> GetStrAstValue(const pypa::AstPtr& ast) {
  if (ast->type != pypa::AstType::Str) {
    return CreateAstError(ast, "Expected string type. Got $0", GetAstTypeName(ast->type));
  }
  return PYPA_PTR_CAST(Str, ast)->value;
}

Status WrapAstError(const pypa::AstPtr& ast, Status status) {
  if (status.ok() || status.has_context()) {
    return status;
  }

  return CreateAstError(ast, status.msg());
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
