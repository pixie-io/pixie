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

#include "src/carnot/planner/ir/expression_ir.h"

namespace px {
namespace carnot {
namespace planner {

struct ColumnExpression {
  ColumnExpression(std::string col_name, ExpressionIR* expr) : name(col_name), node(expr) {}
  std::string name;
  ExpressionIR* node;
};
using ColExpressionVector = std::vector<ColumnExpression>;

}  // namespace planner
}  // namespace carnot
}  // namespace px
