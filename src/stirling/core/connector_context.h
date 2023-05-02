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
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/proc_pid_path.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/utils/proc_tracker.h"

namespace px {
namespace stirling {

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
   * Return true if the UPID is in context.
   */
  virtual bool UPIDIsInContext(const md::UPID& upid) const = 0;

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
   */
  // TODO(oazizi): Consider breaking up into GetPods() and GetContainers().
  virtual const md::K8sMetadataState& GetK8SMetadata() = 0;

  /**
   * Return a list of cluster CIDRs. Any IPs within any of these CIDRs is considered
   * part of the traceable domain. Primarily used to determine when to apply client vs server side
   * tracing.
   */
  virtual std::vector<CIDRBlock> GetClusterCIDRs() = 0;

  virtual void RefreshUPIDList() = 0;
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
  }

  uint32_t GetASID() const override { return agent_metadata_state_->asid(); }

  bool UPIDIsInContext(const md::UPID& upid) const override {
    return agent_metadata_state_->upids().contains(upid);
  }

  const absl::flat_hash_set<md::UPID>& GetUPIDs() const override {
    return agent_metadata_state_->upids();
  }

  const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr>& GetPIDInfoMap() const override {
    return agent_metadata_state_->pids_by_upid();
  }

  const md::K8sMetadataState& GetK8SMetadata() override {
    return agent_metadata_state_->k8s_metadata_state();
  }

  std::vector<CIDRBlock> GetClusterCIDRs() override;

  void RefreshUPIDList() override{};

 private:
  std::shared_ptr<const md::AgentMetadataState> agent_metadata_state_;
};

/**
 * This context is used when Stirling is running stand-alone, not as part of PEM.
 * See specific variants below.
 */
class StandaloneContext : public ConnectorContext {
 public:
  explicit StandaloneContext(absl::flat_hash_set<md::UPID> upids,
                             const std::filesystem::path& proc_path = ::px::system::ProcPath());

  uint32_t GetASID() const override { return 0; }

  bool UPIDIsInContext(const md::UPID& upid) const override { return upids_.contains(upid); }

  const absl::flat_hash_set<md::UPID>& GetUPIDs() const override { return upids_; }

  const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr>& GetPIDInfoMap() const override {
    return upid_pidinfo_map_;
  }

  const md::K8sMetadataState& GetK8SMetadata() override {
    static const md::K8sMetadataState kEmpty;
    return kEmpty;
  }

  std::vector<CIDRBlock> GetClusterCIDRs() override { return cidrs_; }

  Status SetClusterCIDR(std::string_view cidr_str);

  void RefreshUPIDList() override{};

 protected:
  absl::flat_hash_set<md::UPID> upids_;
  absl::flat_hash_map<md::UPID, md::PIDInfoUPtr> upid_pidinfo_map_;

 private:
  std::vector<CIDRBlock> cidrs_;
};

/**
 * This context is used to trace all processes on the system.
 */
class SystemWideStandaloneContext : public StandaloneContext {
 public:
  explicit SystemWideStandaloneContext(
      const std::filesystem::path& proc_path = ::px::system::ProcPath());
};

/**
 * Trace everything. Does NOT filter by whether or not the PID was found in /proc.
 * Useful to trace short lived processes (which SystemWideStandaloneContext may not).
 */
class EverythingLocalContext : public SystemWideStandaloneContext {
 public:
  bool UPIDIsInContext(const md::UPID& /*upid*/) const override { return true; }
  void RefreshUPIDList() override;
};

}  // namespace stirling
}  // namespace px
