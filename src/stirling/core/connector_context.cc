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

}  // namespace stirling
}  // namespace pl
