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
namespace distributed {

/**
 * @brief Ensures that all Scalar UDFs in this plan can run on a PEM.
 */
class ScalarUDFsRunOnPEMRule : public Rule {
 public:
  explicit ScalarUDFsRunOnPEMRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  // Returns true if a given operator's UDFs can successfully run on a PEM.
  static StatusOr<bool> OperatorUDFsRunOnPEM(CompilerState* compiler_state, OperatorIR* op);

 protected:
  StatusOr<bool> Apply(IRNode* node) override;
};

/**
 * @brief Ensures that all Scalar UDFs in this plan can run on a Kelvin.
 */
class ScalarUDFsRunOnKelvinRule : public Rule {
 public:
  explicit ScalarUDFsRunOnKelvinRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  // Returns true if a given operator's UDFs can successfully run on a Kelvin.
  static StatusOr<bool> OperatorUDFsRunOnKelvin(CompilerState* compiler_state, OperatorIR* op);

 protected:
  StatusOr<bool> Apply(IRNode* node) override;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
