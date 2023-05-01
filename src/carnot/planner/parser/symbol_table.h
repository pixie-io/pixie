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
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/ast_utils.h"
#include "src/carnot/planner/ir/ir.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief Symbol Table is an abstraction used to access any table structure.
 * The two main virtual functions to implement are HasSymbol and GetSymbol.
 *
 * AddSymbol is deliberately left out from the interface because there is a case
 * where we would like to store QLObjectPtrs but can't create a circular dependency in the system to
 * access them
 *
 */
class SymbolTable {
 public:
  bool HasSymbol(const std::string& name);
  IRNode* GetSymbol(const std::string& name);

 protected:
  bool HasSymbolImpl(const)
};
using SymbolTablePtr = std::shared_ptr<SymbolTable>;

}  // namespace planner
}  // namespace carnot
}  // namespace px
