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

#include <cmath>
#include <limits>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

udf::ScalarUDFDocBuilder AddDoc();
template <typename TReturn, typename TArg1, typename TArg2>
class AddUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val + b2.val; }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::InheritTypeFromArgs<AddUDF>::Create({types::ST_BYTES, types::ST_THROUGHPUT_PER_NS,
                                                  types::ST_THROUGHPUT_BYTES_PER_NS,
                                                  types::ST_DURATION_NS}),
    };
  }

  static udf::ScalarUDFDocBuilder Doc() { return AddDoc(); }
};

template <>
class AddUDF<types::StringValue, types::StringValue, types::StringValue> : public udf::ScalarUDF {
 public:
  types::StringValue Exec(FunctionContext*, types::StringValue b1, types::StringValue b2) {
    return b1 + b2;
  }

  static udf::ScalarUDFDocBuilder Doc() { return AddDoc(); }
};

void RegisterSecurityFuncsOrDie(udf::Registry* registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace px
