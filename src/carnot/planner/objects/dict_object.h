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
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class DictObject : public QLObject {
 public:
  static constexpr TypeDescriptor DictType = {
      /* name */ "dict",
      /* type */ QLObjectType::kDict,
  };

  static bool IsDict(const QLObjectPtr& object) { return object->type() == DictType.type(); }

  static StatusOr<std::shared_ptr<DictObject>> Create(const pypa::AstPtr& ast,
                                                      const std::vector<QLObjectPtr>& keys,
                                                      const std::vector<QLObjectPtr>& values,
                                                      ASTVisitor* visitor) {
    return std::shared_ptr<DictObject>(new DictObject(ast, keys, values, visitor));
  }

  const std::vector<QLObjectPtr>& keys() const { return *keys_; }
  const std::vector<QLObjectPtr>& values() const { return *values_; }

 protected:
  DictObject(const pypa::AstPtr& ast, const std::vector<QLObjectPtr>& keys,
             const std::vector<QLObjectPtr>& values, ASTVisitor* visitor)
      : QLObject(DictType, ast, visitor) {
    keys_ = std::make_shared<std::vector<QLObjectPtr>>(keys);
    values_ = std::make_shared<std::vector<QLObjectPtr>>(values);
  }

 private:
  std::shared_ptr<std::vector<QLObjectPtr>> keys_;
  std::shared_ptr<std::vector<QLObjectPtr>> values_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
