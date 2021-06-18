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

#include "src/stirling/source_connectors/socket_tracer/socket_trace_bpf_tables.h"

#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/socket_tracer/conn_trackers_manager.h"
#include "src/stirling/utils/proc_path_tools.h"

DEFINE_uint32(stirling_conn_map_cleanup_threshold, kMaxConnMapCleanupItems,
              "Number of map cleanup entries to accumulate before triggering a BPF map clean-up. "
              "Higher numbers result in more efficiency. Too high a number will cause a BPF error "
              "because of the instruction count limit..");

// A function which we will uprobe on, to trigger our BPF code.
// The function itself is irrelevant, but it must not be optimized away.
// We declare this with C linkage (extern "C") so it has a simple symbol name.
extern "C" {
NO_OPT_ATTR void ConnInfoMapCleanupTrigger(int n, struct conn_id_t* conn_id_vec) {
  PL_UNUSED(n);
  PL_UNUSED(conn_id_vec);
  return;
}
}

namespace px {
namespace stirling {

ConnInfoMapManager::ConnInfoMapManager(bpf_tools::BCCWrapper* bcc)
    : conn_info_map_(bcc->GetHashTable<uint64_t, struct conn_info_t>("conn_info_map")),
      conn_disabled_map_(bcc->GetHashTable<uint64_t, uint64_t>("conn_disabled_map")),
      open_file_map_(bcc->GetHashTable<uint64_t, uint64_t>("open_file_map")) {
  // Use address instead of symbol to specify this probe,
  // so that even if debug symbols are stripped, the uprobe can still attach.
  uint64_t symbol_addr = reinterpret_cast<uint64_t>(&ConnInfoMapCleanupTrigger);

  std::filesystem::path self_path = GetSelfPath().ValueOrDie();

  bpf_tools::UProbeSpec uprobe{.binary_path = self_path,
                               .symbol = {},  // Keep GCC happy.
                               .address = symbol_addr,
                               .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
                               .probe_fn = "conn_cleanup_uprobe"};

  PL_CHECK_OK(bcc->AttachUProbe(uprobe));
}

void ConnInfoMapManager::ReleaseResources(struct conn_id_t conn_id) {
  pending_release_queue_.push_back(conn_id);

  if (pending_release_queue_.size() == FLAGS_stirling_conn_map_cleanup_threshold) {
    ConnInfoMapCleanupTrigger(pending_release_queue_.size(), pending_release_queue_.data());
    pending_release_queue_.clear();
  }
}

void ConnInfoMapManager::Disable(struct conn_id_t conn_id) {
  uint64_t key = id(conn_id);

  if (!conn_disabled_map_.update_value(key, conn_id.tsid).ok()) {
    VLOG(1) << absl::Substitute("$0 Updating conn_disable_map entry failed.", ToString(conn_id));
  }
}

void ConnInfoMapManager::CleanupBPFMapLeaks(ConnTrackersManager* conn_trackers_mgr) {
  const auto& sysconfig = system::Config::GetInstance();

  for (const auto& [pid_fd, conn_info] : conn_info_map_.get_table_offline()) {
    uint32_t pid = pid_fd >> 32;
    int32_t fd = pid_fd;

    // Check conn trackers to see if it's already tracked.
    // This is a performance optimization to avoid accessing /proc when not required.
    if (conn_trackers_mgr->GetConnTracker(pid, fd).ok()) {
      continue;
    }

    std::filesystem::path fd_file =
        sysconfig.proc_path() / std::to_string(pid) / "fd" / std::to_string(fd);

    if (fs::Exists(fd_file).ok()) {
      continue;
    }

    ReleaseResources(conn_info.conn_id);
    VLOG(1) << absl::Substitute("Found conn_info_map leak: pid=$0 fd=$1 af=$2", pid, fd,
                                conn_info.addr.sa.sa_family);
  }

  for (const auto& [pid_fd, _] : open_file_map_.get_table_offline()) {
    uint32_t pid = pid_fd >> 32;
    int32_t fd = pid_fd;

    std::filesystem::path fd_file =
        sysconfig.proc_path() / std::to_string(pid) / "fd" / std::to_string(fd);
    if (fs::Exists(fd_file).ok()) {
      continue;
    }

    // TODO(yzhao): Rewrite to use the uprobe-style cleanup.
    open_file_map_.remove_value(pid_fd);
    VLOG(1) << absl::Substitute("Found open_file_map leak: pid=$0 fd=$1", pid, fd);
  }
}

}  // namespace stirling
}  // namespace px
