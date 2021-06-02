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

#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief This rule ensures that all sinks have unique names by appending
 * '_1', '_2', and so on ... to duplicates.
 *
 */
class UniqueSinkNameRule : public Rule {
 public:
  UniqueSinkNameRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  absl::flat_hash_map<std::string, int64_t> sink_names_count_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
