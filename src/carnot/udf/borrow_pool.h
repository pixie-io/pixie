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
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>

namespace px {
namespace carnot {
namespace udf {

template <typename T>
class BorrowPool {
 public:
  using StoredPtrType = std::unique_ptr<T>;
  struct ReclaimDeleter {
    void operator()(T* ptr) {
      if (ptr != nullptr) {
        absl::base_internal::SpinLockHolder l(pool_lock_);
        StoredPtrType stored_ptr(ptr);
        pool_->push_back(std::move(stored_ptr));
      }
    }
    std::vector<StoredPtrType>* pool_;
    absl::base_internal::SpinLock* pool_lock_;
  };
  using BorrowedPtrType = std::unique_ptr<T, ReclaimDeleter>;

  void Add(StoredPtrType ptr) {
    absl::base_internal::SpinLockHolder l(&pool_lock_);
    pool_.push_back(std::move(ptr));
  }

  BorrowedPtrType Borrow() {
    absl::base_internal::SpinLockHolder l(&pool_lock_);
    if (pool_.size() == 0) {
      return nullptr;
    }
    auto raw_ptr = pool_.back().release();
    pool_.pop_back();
    BorrowedPtrType ptr(raw_ptr, ReclaimDeleter{&pool_, &pool_lock_});
    return ptr;
  }

  size_t Size() {
    absl::base_internal::SpinLockHolder l(&pool_lock_);
    return pool_.size();
  }

 private:
  std::vector<StoredPtrType> pool_ GUARDED_BY(pool_lock_);
  absl::base_internal::SpinLock pool_lock_;
};

}  // namespace udf
}  // namespace carnot
}  // namespace px
