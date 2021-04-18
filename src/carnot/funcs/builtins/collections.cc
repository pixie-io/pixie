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

#include "src/carnot/funcs/builtins/collections.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterCollectionOpsOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<AnyUDA<types::BoolValue>>("any");
  registry->RegisterOrDie<AnyUDA<types::Int64Value>>("any");
  registry->RegisterOrDie<AnyUDA<types::Float64Value>>("any");
  registry->RegisterOrDie<AnyUDA<types::Time64NSValue>>("any");
  registry->RegisterOrDie<AnyUDA<types::StringValue>>("any");
  registry->RegisterOrDie<AnyUDA<types::UInt128Value>>("any");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
