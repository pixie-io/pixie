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

#include <cstdint>
#include <vector>

#include <magic_enum.hpp>

namespace px {
namespace stirling {
namespace utils {

/**
 * Maintains a very simple mapping from enum keys and integer counters.
 * Internally it uses a vector instead of a map for faster access.
 * So the enum values of the key type cannot have custom values.
 */
template <typename TKeyType>
class StatCounter {
 public:
  void Increment(TKeyType key, int count = 1) { counts_[static_cast<int>(key)] += count; }
  void Decrement(TKeyType key, int count = 1) { counts_[static_cast<int>(key)] -= count; }
  void Set(TKeyType key, int count) { counts_[static_cast<int>(key)] = count; }
  int64_t Get(TKeyType key) const { return counts_[static_cast<int>(key)]; }

 private:
  std::vector<int64_t> counts_ = std::vector<int64_t>(magic_enum::enum_count<TKeyType>(), 0);
};

}  // namespace utils
}  // namespace stirling
}  // namespace px
