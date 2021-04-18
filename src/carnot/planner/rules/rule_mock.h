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

#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
class MockRule : public Rule {
 public:
  explicit MockRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}
  MockRule() : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  MOCK_METHOD1(Execute, StatusOr<bool>(IR* ir_graph));

 protected:
  MOCK_METHOD1(Apply, StatusOr<bool>(IRNode* ir_node));
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
