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

#include <map>
#include <utility>

#include <magic_enum.hpp>

namespace px {

/**
 * This function converts an enum definition to a map where the key is the value,
 * and the name is the name of the enum option.
 *
 * Used for decoding raw values of an enum to a human readable name.
 *
 * @tparam TEnumType The enum to encode into the map representation.
 * @return A map that defines the enum declaration.
 */
template <typename TEnumType>
std::map<int64_t, std::string_view> EnumDefToMap() {
  constexpr int kEnumCount = magic_enum::enum_count<TEnumType>();
  std::array<std::pair<TEnumType, std::string_view>, kEnumCount> entries =
      magic_enum::enum_entries<TEnumType>();

  // Convert magic_enum array to a map, which is easier to lookup.
  std::map<int64_t, std::string_view> result;
  for (const auto& e : entries) {
    result[static_cast<int64_t>(e.first)] = e.second;
  }

  return result;
}

}  // namespace px
