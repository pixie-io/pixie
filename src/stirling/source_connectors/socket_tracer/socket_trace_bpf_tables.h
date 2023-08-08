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
#include <string>
#include <vector>

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

DECLARE_uint32(stirling_conn_map_cleanup_threshold);

namespace px {
namespace stirling {

using px::stirling::bpf_tools::WrappedBCCMap;

// Forward declaration.
class ConnTrackersManager;

class ConnInfoMapManager {
 public:
  explicit ConnInfoMapManager(bpf_tools::BCCWrapper* bcc);

  void ReleaseResources(struct conn_id_t conn_id);

  void Disable(struct conn_id_t conn_id);

  void CleanupBPFMapLeaks(ConnTrackersManager* conn_trackers_mgr);

 private:
  std::unique_ptr<WrappedBCCMap<uint64_t, struct conn_info_t>> conn_info_map_;
  std::unique_ptr<WrappedBCCMap<uint64_t, uint64_t>> conn_disabled_map_;

  std::vector<struct conn_id_t> pending_release_queue_;

  // TODO(oazizi): Can we share this with the similar function in socket_trace.c?
  uint64_t id(struct conn_id_t conn_id) const {
    return (static_cast<uint64_t>(conn_id.upid.tgid) << 32) | conn_id.fd;
  }
};

}  // namespace stirling
}  // namespace px
