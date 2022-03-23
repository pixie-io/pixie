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
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief NoneObject represents None in python, the "null" object. This is used as a proxy for void
 * return type in Python interpretation.
 *
 */
class NoneObject : public QLObject {
 public:
  static constexpr TypeDescriptor NoneType = {
      /* name */ "None",
      /* type */ QLObjectType::kNone,
  };

  /**
   * @brief Construct a None object that represents the null value in Python.
   *
   * @param ast the ast ptr for the
   */
  NoneObject(pypa::AstPtr ast, ASTVisitor* ast_visitor) : QLObject(NoneType, ast, ast_visitor) {}
  explicit NoneObject(ASTVisitor* ast_visitor) : QLObject(NoneType, ast_visitor) {}
  bool CanAssignAttribute(std::string_view /*attr_name*/) const override { return false; }

  static bool IsNoneObject(const QLObjectPtr& obj) { return obj->type() == NoneType.type(); }
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
