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

namespace px {
namespace carnot {
namespace udf {

enum ModelType {
  kTransformer,
};

/**
 * Base class for all ModelExecutors.
 *
 * Subclasses should implement a static Type method with signature
 *  static constexpr ModelType Type();
 */
class ModelExecutor {
 public:
  virtual ~ModelExecutor() = default;
};

}  // namespace udf
}  // namespace carnot
}  // namespace px
