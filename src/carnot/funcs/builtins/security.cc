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
// udf::ScalarUDFDocBuilder AddDoc() {
//   return udf::ScalarUDFDocBuilder("Get all values inside the given PostgreSQL query string.")
//       .Details(R"(Returns all values in the form of the list from the PostgreSQL query string.)")
//       .Example(R"doc(
//         | df.GetValuesFromPostgreSQLQuery("select * from tbl where 'id'=123 OR 1=1")
// 		| ["*","123","1","1"] ))doc")
//       .Arg("sqlQuery", "The query string to be evaluated.")
//       .Returns("Returns all values inside the given PostgreSQL query string.");
// }

void RegisterSecurityFuncsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  registry->RegisterOrDie<AddUDF<types::StringValue, types::StringValue, types::StringValue>>(
      "add");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
