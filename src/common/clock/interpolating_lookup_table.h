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

#include <deque>
#include <utility>
#include "absl/base/internal/spinlock.h"

#include "src/common/base/base.h"

namespace px {
namespace clock {

/**
 * InterpolatingLookupTable stores sorted key,value pairs in a circular buffer.
 * When accessing the map, if the key is not in the map, the closest key,value pairs are
 * interpolated to get a corresponding value.
 */
template <size_t TCapacity>
class InterpolatingLookupTable {
  // The Map stores a mapping from key to offset (i.e from key to (value - key))
  using MapPairType = std::pair<uint64_t, int64_t>;
  using CircularMap = std::deque<MapPairType>;

 public:
  void Emplace(uint64_t key, uint64_t val) {
    absl::base_internal::SpinLockHolder lock(&buffer_lock_);
    if (buffer_.size() > TCapacity) {
      buffer_.pop_front();
    }
    buffer_.push_back(std::make_pair(key, static_cast<int64_t>(val) - key));
  }

  uint64_t Get(uint64_t key) const {
    {
      absl::base_internal::SpinLockHolder lock(&buffer_lock_);
      if (buffer_.size() == 0) {
        return key;
      }
    }

    MapPairType a;
    MapPairType b;
    auto found_two_points = GetLeftRightInterpolationPoints(key, &a, &b);
    if (!found_two_points) {
      // If we are before or after the history we have stored we just use the closest offset we can.
      return key + a.second;
    }
    return key + LinearInterpolate(a.first, b.first, a.second, b.second, key);
  }

  size_t size() const {
    absl::base_internal::SpinLockHolder lock(&buffer_lock_);
    return buffer_.size();
  }

 private:
  mutable absl::base_internal::SpinLock buffer_lock_;
  CircularMap buffer_ ABSL_GUARDED_BY(buffer_lock_);

  bool GetLeftRightInterpolationPoints(uint64_t key, MapPairType* left, MapPairType* right) const {
    absl::base_internal::SpinLockHolder lock(&buffer_lock_);
    if (buffer_.size() == 1) {
      *left = *buffer_.begin();
      return false;
    }
    auto cmp = [](MapPairType a, uint64_t b) { return a.first < b; };
    auto it = std::lower_bound(buffer_.begin(), buffer_.end(), key, cmp);
    if (it == buffer_.end()) {
      *left = *std::prev(buffer_.end());
      return false;
    }
    if (it == buffer_.begin()) {
      *left = *buffer_.begin();
      return false;
    }
    if (it->first == key) {
      *left = *it;
      return false;
    }
    *left = *std::prev(it);
    *right = *it;
    return true;
  }
};

}  // namespace clock
}  // namespace px
