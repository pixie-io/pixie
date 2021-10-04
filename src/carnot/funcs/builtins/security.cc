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

#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {
udf::ScalarUDFDocBuilder AddDoc() {
  return udf::ScalarUDFDocBuilder("Arithmetically add the arguments or concatenate the strings.")
      .Details(R"(This function is implicitly invoked by the + operator.
      If both types are strings, then will concate the strings. Trying to
      add any other type to a string will cause an error)")
      .Example(R"doc(# Implicit call.
        | df.sum = df.a + df.b
        | Explicit call.
        | df.sum = px.add(df.a, df.b))doc")
      .Arg("a", "The value to be added to.")
      .Arg("b", "The value to add to the first argument.")
      .Returns("The sum of a and b.");
}

void RegisterSecurityFuncsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  registry->RegisterOrDie<AddUDF<types::StringValue, types::StringValue, types::StringValue>>(
      "add");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
