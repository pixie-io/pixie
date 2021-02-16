#include "src/stirling/core/connector_context.h"

namespace pl {
namespace stirling {

std::vector<CIDRBlock> AgentContext::GetClusterCIDRs() {
  std::vector<CIDRBlock> cluster_cidrs;

  // Copy Pod CIDRs.
  const std::vector<CIDRBlock>& pod_cidrs = agent_metadata_state_->k8s_metadata_state().pod_cidrs();
  for (const auto& pod_cidr : pod_cidrs) {
    cluster_cidrs.push_back(pod_cidr);
  }

  // Copy Service CIDRs.
  const std::optional<CIDRBlock>& service_cidr =
      agent_metadata_state_->k8s_metadata_state().service_cidr();
  if (service_cidr.has_value()) {
    cluster_cidrs.push_back(service_cidr.value());
  }

  return cluster_cidrs;
}

Status StandaloneContext::SetClusterCIDR(std::string_view cidr_str) {
  CIDRBlock cidr;
  Status s = ParseCIDRBlock(cidr_str, &cidr);
  if (!s.ok()) {
    return error::Internal("Could not parse $0 as a CIDR.", cidr_str);
  }
  cidrs_ = {std::move(cidr)};
  return Status::OK();
}

absl::flat_hash_set<md::UPID> ListUPIDs(const std::filesystem::path& proc_path, uint32_t asid) {
  absl::flat_hash_set<md::UPID> pids;
  for (const auto& p : std::filesystem::directory_iterator(proc_path)) {
    uint32_t pid = 0;
    if (!absl::SimpleAtoi(p.path().filename().string(), &pid)) {
      continue;
    }
    StatusOr<int64_t> pid_start_time = system::GetPIDStartTimeTicks(p.path());
    if (!pid_start_time.ok()) {
      VLOG(1) << absl::Substitute("Could not get PID start time for pid $0. Likely already dead.",
                                  p.path().string());
      continue;
    }
    pids.emplace(asid, pid, pid_start_time.ValueOrDie());
  }
  return pids;
}

}  // namespace stirling
}  // namespace pl
