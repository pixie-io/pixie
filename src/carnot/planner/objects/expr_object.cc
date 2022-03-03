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

#include <memory>
#include <string>

#include "src/carnot/planner/objects/expr_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::string> GetAsString(const QLObjectPtr& obj) {
  if (!ExprObject::IsExprObject(obj)) {
    return obj->CreateError("Expected string, received $0", obj->name());
  }
  auto expr = static_cast<ExprObject*>(obj.get())->expr();
  if (!StringIR::NodeMatches(expr)) {
    return obj->CreateError("Expected string, received $0", obj->name());
  }

  return static_cast<StringIR*>(expr)->str();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
