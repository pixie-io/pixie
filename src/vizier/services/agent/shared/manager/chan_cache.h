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

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <string>

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_map.h>
#include "src/common/base/base.h"

namespace px {
namespace vizier {
namespace agent {

class ChanCache {
 public:
  /**
   * @brief Construct a new ChanChache.
   *
   * @param warm_up_period the period to wait in a Channel's lifetime before we declare an idle
   * channel to be out of use. This is in place to prevent a race where we add a Chan and
   * CleanupChans() is called before the Connection can be used, meaning the channel will come up
   * idle.
   */
  explicit ChanCache(std::chrono::nanoseconds warm_up_period) : warm_up_period_(warm_up_period) {}
  // Template to handle other duration types.
  template <typename T>
  explicit ChanCache(std::chrono::duration<int64_t, T> warm_up_period)
      : ChanCache(std::chrono::duration_cast<std::chrono::nanoseconds>(warm_up_period)) {}
  /**
   * @brief Gets the Chan at remote_addr. If the cache doesn't contain the channel, it returns a
   * nullptr.
   *
   * @param remote_addr the remote_address to look up.
   * @return std::shared_ptr<::grpc::Channel> the channel or a nullptr if not found.
   */
  std::shared_ptr<::grpc::Channel> GetChan(std::string_view remote_addr);

  /**
   * @brief Caches `chan` for the `remote_addr`.
   *
   * @param remote_addr the remote address corresponding to the channel.
   * @param chan the channel to cache.
   */
  void Add(std::string remote_addr, std::shared_ptr<::grpc::Channel> chan);

  /**
   * @brief Goes through the cached channels and verify whether they are still alive and have been
   * used recently. We consider a connection out of use if it is in a failure state or is idle and
   * the connection is not in the warm up period.
   *
   * The period where a connection goes from READY to IDLE can be adjusted with the channel
   * argument: GRPC_ARG_CLIENT_IDLE_TIMEOUT_MS. See the test for an example.
   *
   * @return Status: whether an error occured while cleaning up Idle/Dead Channels.
   */
  Status CleanupChans();

 private:
  struct Channel {
    std::shared_ptr<::grpc::Channel> chan;
    std::chrono::system_clock::time_point start_time;
  };

  // The cache of channels (grpc conns) made to other agents.
  absl::flat_hash_map<std::string, Channel> chan_cache_ GUARDED_BY(chan_cache_lock_);
  absl::base_internal::SpinLock chan_cache_lock_;
  // Connections that are alive for shorter than warm_up_period_ won't be cleared.
  std::chrono::nanoseconds warm_up_period_;
};
}  // namespace agent
}  // namespace vizier
}  // namespace px
