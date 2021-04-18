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

#include "src/carnot/udf/registry.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

/**
 * Registers UDF operations that work on conditionals
 * @param registry pointer to the registry.
 */
void RegisterConditionalOpsOrDie(udf::Registry* registry);

template <typename TArg>
class SelectUDF : public udf::ScalarUDF {
 public:
  TArg Exec(FunctionContext*, BoolValue s, TArg v1, TArg v2) {
    // TODO(zasgar): This does do eager evaluation of both sides./
    // If we start thinking about code generation, we can probably optimize this.
    if (s.val) {
      return v1;
    }
    return v2;
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    // Match the 1st and 2nd arg.
    return {udf::InheritTypeFromArgs<SelectUDF>::CreateGeneric({1, 2})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Selects value based on the first argument.")
        .Example(R"doc(
        | # Explicit call.
        | df.val = px.select(df.select_left, df.left, df.right)
        )doc")
        .Arg("s", "The selector value")
        .Arg("v1", "The return value when s is true")
        .Arg("v2", "The return value when s is false")
        .Returns("Return v1 if s else v2");
  }
};

}  // namespace builtins
}  // namespace carnot
}  // namespace px
