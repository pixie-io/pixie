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

#include "src/carnot/funcs/builtins/json_ops.h"

#include "src/carnot/udf/registry.h"

namespace px {
namespace carnot {
namespace builtins {

using types::StringValue;

void RegisterJSONOpsOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<PluckUDF>("pluck");
  registry->RegisterOrDie<PluckAsInt64UDF>("pluck_int64");
  registry->RegisterOrDie<PluckAsFloat64UDF>("pluck_float64");
  registry->RegisterOrDie<PluckArrayUDF>("pluck_array");

  // Up to 8 script args are supported for the _script_reference UDF, due to the lack of support for
  // variadic UDF arguments in the UDF registry today. We should clean this up if/when variadic UDF
  // arguments are supported, which will probably be done as a part of adding support for object
  // types. 0 script args
  registry->RegisterOrDie<ScriptReferenceUDF<>>("_script_reference");
  // 1 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue>>("_script_reference");
  // 2 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue>>(
      "_script_reference");
  // 3 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue>>("_script_reference");
  // 4 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue, StringValue, StringValue>>(
      "_script_reference");
  // 5 script args
  registry->RegisterOrDie<
      ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue, StringValue,
                         StringValue, StringValue, StringValue, StringValue, StringValue>>(
      "_script_reference");
  // 6 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue, StringValue, StringValue>>(
      "_script_reference");
  // 7 script args
  registry->RegisterOrDie<ScriptReferenceUDF<
      StringValue, StringValue, StringValue, StringValue, StringValue, StringValue, StringValue,
      StringValue, StringValue, StringValue, StringValue, StringValue, StringValue, StringValue>>(
      "_script_reference");
  // 8 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue, StringValue, StringValue>>(
      "_script_reference");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
