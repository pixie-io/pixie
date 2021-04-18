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

#include <memory>

namespace px {
namespace carnot {
namespace udf {
// Forward declare registries.
class Registry;
}  // namespace udf
namespace plan {

class PlanState {
 public:
  /**
   * Init with a UDF registry and hold a raw pointer for it.
   *
   * @param func_registry the passed in UDF registry.
   */
  explicit PlanState(udf::Registry* func_registry) : func_registry_(func_registry) {}

  udf::Registry* func_registry() const { return func_registry_; }

 private:
  udf::Registry* func_registry_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace px
