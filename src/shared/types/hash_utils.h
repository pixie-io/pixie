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

#include <farmhash.h>

#include <cstdint>

#include "src/shared/types/types.h"

namespace px {
namespace types {
namespace utils {

// PX_CARNOT_UPDATE_FOR_NEW_TYPES
template <typename T>
struct hash {};

template <>
struct hash<BoolValue> {
  uint64_t operator()(BoolValue val) { return static_cast<uint64_t>(val.val); }
};

template <>
struct hash<Int64Value> {
  uint64_t operator()(Int64Value val) {
    return ::util::Hash64(reinterpret_cast<const char*>(&(val.val)), sizeof(int64_t));
  }
};

template <>
struct hash<Float64Value> {
  uint64_t operator()(Float64Value val) {
    return ::util::Hash64(reinterpret_cast<const char*>(&(val.val)), sizeof(double));
  }
};

template <>
struct hash<StringValue> {
  uint64_t operator()(StringValue val) { return ::util::Hash64(val); }
};

template <>
struct hash<Time64NSValue> {
  uint64_t operator()(Time64NSValue val) {
    return ::util::Hash64(reinterpret_cast<const char*>(&(val)), sizeof(Time64NSValue));
  }
};

}  // namespace utils
}  // namespace types
}  // namespace px
