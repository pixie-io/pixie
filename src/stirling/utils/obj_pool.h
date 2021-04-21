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

namespace px {
namespace stirling {

/**
 * ObjPool manages a pool of objects that can be recycled to avoid memory reallocations.
 */
template <typename T>
class ObjPool {
 public:
  explicit ObjPool(size_t capacity) : capacity_(capacity) { obj_pool_.reserve(capacity_); }

  ~ObjPool() {
    // There is no placement delete, so we have to perform a placement new
    // followed by a full delete. A bit wasteful, but this is only run on termination anyways.
    for (auto& x : obj_pool_) {
      new (x) T();
      delete (x);
    }
    obj_pool_.clear();
  }

  /**
   * Pop() returns either a new or recycled object.
   * If recycled, it will be properly initialized.
   */
  std::unique_ptr<T> Pop() {
    if (obj_pool_.empty()) {
      auto obj_ptr = std::make_unique<T>();
      VLOG(1) << absl::Substitute("Pool is empty...creating new object [addr=$0].", obj_ptr.get());
      return obj_ptr;
    }

    VLOG(1) << absl::Substitute("Retrieving object from recycle pool [addr=$0].", obj_pool_.back());
    // The objects's memory was never released, but we still to initialize the object
    // as though it is new, so we use C++ "placement new" to do so.
    // This avoids a new memory allocation, but does still initialize the object.
    auto obj = std::unique_ptr<T>(new (obj_pool_.back()) T());
    obj_pool_.pop_back();
    return obj;
  }

  /**
   * Recycle() submits an object for recycling.
   * The pool may choose to add it for recycling or to deallocate it.
   */
  void Recycle(std::unique_ptr<T> obj) {
    if (obj_pool_.size() >= capacity_) {
      VLOG(1) << absl::Substitute("Pool is at capacity...deallocating object [addr=$0].",
                                  obj.get());
      return;
    }

    VLOG(1) << absl::Substitute("Adding object to recycle pool [addr=$0].", obj.get());
    T* obj_ptr = obj.release();
    obj_pool_.push_back(obj_ptr);
    obj_ptr->~T();
  }

 private:
  size_t capacity_;
  std::vector<T*> obj_pool_;
};

}  // namespace stirling
}  // namespace px
