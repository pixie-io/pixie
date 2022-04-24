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

#include "src/carnot/funcs/builtins/string_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterStringOpsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  // String contains.
  registry->RegisterOrDie<ContainsUDF>("contains");
  registry->RegisterOrDie<LengthUDF>("length");
  registry->RegisterOrDie<FindUDF>("find");
  registry->RegisterOrDie<SubstringUDF>("substring");
  registry->RegisterOrDie<ToLowerUDF>("tolower");
  registry->RegisterOrDie<ToUpperUDF>("toupper");
  registry->RegisterOrDie<TrimUDF>("trim");
  registry->RegisterOrDie<StripPrefixUDF>("strip_prefix");
  registry->RegisterOrDie<HexToASCII>("hex_to_ascii");
  registry->RegisterOrDie<BytesToHex>("bytes_to_hex");
  registry->RegisterOrDie<StringToIntUDF>("atoi");
  registry->RegisterOrDie<IntToStringUDF>("itoa");
  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
