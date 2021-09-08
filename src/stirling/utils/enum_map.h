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

#include <optional>
#include <vector>

#include <absl/container/flat_hash_set.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace utils {

/**
 * A map-like data structure backed by vector. The key type must be enum type.
 * NOTE: This does not work for bool as value type, as the std::vector<bool> is specialized and is
 * incompatible with the API.
 */
template <typename TEnumKeyType, typename TValueType>
class EnumMap {
 public:
  EnumMap() : values_(magic_enum::detail::reflected_max_v<TEnumKeyType>) {}

  bool Set(TEnumKeyType key, TValueType value) {
    if (set_keys_.contains(key)) {
      return false;
    }
    values_[static_cast<int>(key)] = value;
    set_keys_.insert(key);
    return true;
  }

  const auto& Get(TEnumKeyType key) const {
    DCHECK(set_keys_.contains(key));
    return values_[static_cast<int>(key)];
  }

  bool AreAllKeysSet() const {
    for (auto v : magic_enum::enum_values<TEnumKeyType>()) {
      if (!set_keys_.contains(v)) {
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<TValueType> values_;

  // Keys that have been set are inserted to this set for later checking.
  absl::flat_hash_set<TEnumKeyType> set_keys_;
};

}  // namespace utils
}  // namespace stirling
}  // namespace px
