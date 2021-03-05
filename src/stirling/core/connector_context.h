#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/utils/proc_tracker.h"

namespace pl {
namespace stirling {

/**
 * Returns the list of processes from the proc filesystem. Used by StandaloneContext.
 */
absl::flat_hash_set<md::UPID> ListUPIDs(const std::filesystem::path& proc_path, uint32_t asid = 0);

/**
 * ConnectorContext is the information passed on every Transfer call to source connectors.
 */
class ConnectorContext {
 public:
  virtual ~ConnectorContext() = default;

  /**
   * Return the ASID of the context. Should be a constant across calls.
   */
  virtual uint32_t GetASID() const = 0;

  /**
   * Return current set of active UPIDs.
   */
  virtual const absl::flat_hash_set<md::UPID>& GetUPIDs() const = 0;

  /**
   * Return detailed information on UPIDs.
   */
  virtual const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr>& GetPIDInfoMap() const = 0;

  /**
   * Return K8s information (Pod and container information)
   * @return
   */
  // TODO(oazizi): Consider breaking up into GetPods() and GetContainers().
  virtual const md::K8sMetadataState& GetK8SMetadata() = 0;

  /**
   * Return a list of cluster CIDRs. Any IPs within any of these CIDRs is considered
   * part of the traceable domain. Primarily used to determine when to apply client vs server side
   * tracing.
   */
  virtual std::vector<CIDRBlock> GetClusterCIDRs() = 0;
};

/**
 * This Context is used when Stirling is part of the the PEM/Agent.
 */
class AgentContext : public ConnectorContext {
 public:
  /**
   * ConnectorContext with metadata state.
   * @param agent_metadata_state A read-only snapshot view of the metadata state. This state
   * should not be held onto for extended periods of time.
   */
  explicit AgentContext(std::shared_ptr<const md::AgentMetadataState> agent_metadata_state)
      : agent_metadata_state_(std::move(agent_metadata_state)) {
    DCHECK(agent_metadata_state_ != nullptr);
    upids_ = agent_metadata_state_->upids();
  }

  uint32_t GetASID() const override { return agent_metadata_state_->asid(); }

  const absl::flat_hash_set<md::UPID>& GetUPIDs() const override { return upids_; }

  const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr>& GetPIDInfoMap() const override {
    return agent_metadata_state_->pids_by_upid();
  }

  const md::K8sMetadataState& GetK8SMetadata() override {
    return agent_metadata_state_->k8s_metadata_state();
  }

  std::vector<CIDRBlock> GetClusterCIDRs() override;

 private:
  std::shared_ptr<const md::AgentMetadataState> agent_metadata_state_;
  absl::flat_hash_set<md::UPID> upids_;
};

/**
 * This Context is used when Stirling is running stand-alone, not as part of PEM.
 */
class StandaloneContext : public ConnectorContext {
 public:
  StandaloneContext() {
    // The context consists of all PIDs, but no pods/containers.
    upids_ = ListUPIDs(system::Config::GetInstance().proc_path(), 0);

    // Include loopback address as CIDR block.
    // TODO(yzhao): Might need to include IPv6 version when tests for IPv6 are added.
    ECHECK_OK(SetClusterCIDR("127.0.0.1/32"));
  }

  uint32_t GetASID() const override { return 0; }

  const absl::flat_hash_set<md::UPID>& GetUPIDs() const override { return upids_; }

  const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr>& GetPIDInfoMap() const override {
    static const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr> kEmpty;
    return kEmpty;
  }

  const md::K8sMetadataState& GetK8SMetadata() override {
    static const md::K8sMetadataState kEmpty;
    return kEmpty;
  }

  std::vector<CIDRBlock> GetClusterCIDRs() override { return cidrs_; }

  Status SetClusterCIDR(std::string_view cidr_str);

 private:
  std::vector<CIDRBlock> cidrs_;
  absl::flat_hash_set<md::UPID> upids_;
};

}  // namespace stirling
}  // namespace pl
