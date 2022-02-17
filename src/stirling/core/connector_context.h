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
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/utils/proc_tracker.h"

namespace px {
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
  }

  uint32_t GetASID() const override { return agent_metadata_state_->asid(); }

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

 private:
  std::shared_ptr<const md::AgentMetadataState> agent_metadata_state_;
};

/**
 * This context is used when Stirling is running stand-alone, not as part of PEM.
 * See specific variants below.
 */
class StandaloneContext : public ConnectorContext {
 public:
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

 protected:
  // Protected constructor prevents this class from being used directly.
  StandaloneContext() {
    // Cannot be empty, otherwise stirling will wait indefinitely. Since StandaloneContext is used
    // for local environment, set it such that localhost (127.0.0.1) will be treated as outside of
    // cluster, and --treat_loopback_as_in_cluster in conn_tracker.cc will take effect.
    // TODO(yzhao): Might need to include IPv6 version when tests for IPv6 are added.
    ECHECK_OK(SetClusterCIDR("0.0.0.1/32"));
  }

  absl::flat_hash_set<md::UPID> upids_;

 private:
  std::vector<CIDRBlock> cidrs_;
};

/**
 * This context is used to trace all processes on the system.
 */
class SystemWideStandaloneContext : public StandaloneContext {
 public:
  SystemWideStandaloneContext() {
    // The context consists of all PIDs on the system.
    upids_ = ListUPIDs(system::Config::GetInstance().proc_path(), 0);
  }
};

/**
 * This context is used to focus on certain processes, typically for tests.
 */
class TestContext : public StandaloneContext {
 public:
  explicit TestContext(const absl::flat_hash_set<md::UPID>& upids) {
    // The context consists of all PIDs on the system.
    upids_ = upids;
  }
};

}  // namespace stirling
}  // namespace px
