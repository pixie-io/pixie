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

#include <absl/container/flat_hash_set.h>

#include <utility>

#include "src/common/system/proc_parser.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {

/**
 * Keeps a list of UPIDs. Tracks newly-created and terminated processes each time an update is
 * provided, and updates its internal list of UPIDs.
 */
class ProcTracker : NotCopyMoveable {
 public:
  /**
   * Takes the current set of upids, and updates the internal state.
   * @param upids Current set of UPIDs.
   */
  void Update(absl::flat_hash_set<md::UPID> upids);

  /**
   * Returns all current upids, as set by last call to Update().
   */
  const auto& upids() const { return upids_; }

  /**
   * Returns new upids discovered by call to Update().
   */
  const auto& new_upids() const { return new_upids_; }

  /**
   * Returns upids that were deleted between last call the Update(), and the previous state.
   */
  const auto& deleted_upids() const { return deleted_upids_; }

 private:
  absl::flat_hash_set<md::UPID> upids_;
  absl::flat_hash_set<md::UPID> new_upids_;
  absl::flat_hash_set<md::UPID> deleted_upids_;
};

/**
 * Tracks the java processes that are being monitored by Java profiling agent.
 */
class JavaProfilingProcTracker : NotCopyMoveable {
 public:
  /**
   * Returns the pointer to the singleton.
   */
  static JavaProfilingProcTracker* GetSingleton() {
    static JavaProfilingProcTracker singleton;
    return &singleton;
  }

  /**
   * Inserts the upid of a Java process to the list being tracked.
   */
  void Add(struct upid_t upid) { upids_.insert(std::move(upid)); }

  /**
   * Removes the upid of a Java process from the list being tracked.
   */
  void Remove(const struct upid_t& upid) { upids_.erase(upid); }

  const auto& upids() const { return upids_; }

 private:
  absl::flat_hash_set<struct upid_t> upids_;
};

}  // namespace stirling
}  // namespace px
