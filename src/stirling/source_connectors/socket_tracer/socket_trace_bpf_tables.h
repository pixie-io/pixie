#pragma once

#include <string>
#include <vector>

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

DECLARE_uint32(stirling_conn_map_cleanup_threshold);

namespace pl {
namespace stirling {

class ConnInfoMapManager {
 public:
  explicit ConnInfoMapManager(bpf_tools::BCCWrapper* bcc);

  void ReleaseResources(struct conn_id_t conn_id);

  void Disable(struct conn_id_t conn_id);

 private:
  ebpf::BPFHashTable<uint64_t, struct conn_info_t> conn_info_map_;
  ebpf::BPFHashTable<uint64_t, uint64_t> conn_disabled_map_;

  std::vector<struct conn_id_t> pending_release_queue_;

  // TODO(oazizi): Can we share this with the similar function in socket_trace.c?
  uint64_t id(struct conn_id_t conn_id) const {
    return (static_cast<uint64_t>(conn_id.upid.tgid) << 32) | conn_id.fd;
  }
};

}  // namespace stirling
}  // namespace pl
