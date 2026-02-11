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

#include "src/carnot/funcs/builtins/builtins.h"
#include "src/carnot/funcs/builtins/collections.h"
#include "src/carnot/funcs/builtins/conditionals.h"
#include "src/carnot/funcs/builtins/json_ops.h"
#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/funcs/builtins/math_sketches.h"
#include "src/carnot/funcs/builtins/ml_ops.h"
#include "src/carnot/funcs/builtins/pii_ops.h"
#include "src/carnot/funcs/builtins/pprof_ops.h"
#include "src/carnot/funcs/builtins/regex_ops.h"
#include "src/carnot/funcs/builtins/request_path_ops.h"
#include "src/carnot/funcs/builtins/sql_ops.h"
#include "src/carnot/funcs/builtins/string_ops.h"
#include "src/carnot/funcs/builtins/uri_ops.h"
#include "src/carnot/funcs/builtins/util_ops.h"

#include "src/carnot/udf/registry.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterBuiltinsOrDie(udf::Registry* registry) {
  RegisterCollectionOpsOrDie(registry);
  RegisterConditionalOpsOrDie(registry);
  RegisterMathOpsOrDie(registry);
  RegisterMathSketchesOrDie(registry);
  RegisterJSONOpsOrDie(registry);
  RegisterStringOpsOrDie(registry);
  RegisterMLOpsOrDie(registry);
  RegisterRequestPathOpsOrDie(registry);
  RegisterSQLOpsOrDie(registry);
  RegisterRegexOpsOrDie(registry);
  RegisterPIIOpsOrDie(registry);
  RegisterURIOpsOrDie(registry);
  RegisterUtilOpsOrDie(registry);
  RegisterPProfOpsOrDie(registry);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
