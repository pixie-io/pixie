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
namespace event {

/**
 * If an object derives from this class, it can be passed to the dispatcher who guarantees to delete
 * it in a future event loop cycle. This allows clear ownership with unique_ptr while not having
 * to worry about stack unwind issues during event processing.
 */
class DeferredDeletable {
 public:
  virtual ~DeferredDeletable() = default;
};

using DeferredDeletableUPtr = std::unique_ptr<DeferredDeletable>;

}  // namespace event
}  // namespace px
