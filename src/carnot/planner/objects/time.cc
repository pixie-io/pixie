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

#include "src/carnot/planner/objects/time.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<int64_t> ParseAllTimeFormats(int64_t time_now, ExpressionIR* time_expr) {
  if (Match(time_expr, Int())) {
    return static_cast<IntIR*>(time_expr)->val();
  } else if (Match(time_expr, Time())) {
    return static_cast<TimeIR*>(time_expr)->val();
  } else if (Match(time_expr, String())) {
    return ParseStringToTime(static_cast<StringIR*>(time_expr), time_now);
  }
  CHECK(!Match(time_expr, Func()));
  return 0;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
