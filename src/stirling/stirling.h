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

#include <signal.h>

#include <memory>
#include <string>
#include <vector>

#include <sole.hpp>

#include "src/common/base/base.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/utils/linux_headers.h"

DECLARE_string(stirling_sources);

namespace px {
namespace stirling {

/**
 * Specifies a set of sources to initialize as part of a source registry.
 */
enum class SourceConnectorGroup {
  // No static sources.
  kNone,

  // All available sources (including deprecated and test sources).
  kAll,

  // All production sources.
  kProd,

  // All production sources that generate metrics.
  kMetrics,

  // All production sources that are tracers.
  kTracers,

  // The stack trace profiler.
  kProfiler,

  // The TCP conenction TX/RX/Retransmission metrics.
  kTCPStats,
};

/**
 * Returns the set of source names for the specified group.
 */
std::vector<std::string_view> GetSourceNamesForGroup(SourceConnectorGroup group);

/**
 * Returns a source registry registered with the specified sources.
 * The sources are specified by --sources flag.
 */
std::unique_ptr<SourceRegistry> CreateSourceRegistryFromFlag();

/**
 * Returns a source registry registered with all prod sources.
 */
std::unique_ptr<SourceRegistry> CreateProdSourceRegistry();

/**
 * Returns the set of names of all production source connectors.
 */
absl::flat_hash_set<std::string_view> GetProdSourceNames();

/**
 * The data collector collects data from various different 'sources',
 * and makes them available via a structured API, where the data can then be used and queried as
 * needed (by Pixie or others). Its function is to unify various, disparate sources of data into a
 * common, structured data format.
 */
class Stirling : public NotCopyable {
 public:
  Stirling() = default;
  virtual ~Stirling() = default;

  /**
   * Create a Stirling object.
   * Factory method to create Stirling with a source registry.
   *
   * @param registry
   * @return std::unique_ptr<Stirling>
   */
  static std::unique_ptr<Stirling> Create(std::unique_ptr<SourceRegistry> registry);

  /**
   * Registers debug handlers on USR1/USR2.
   * Will clobber any existing handlers, so make sure no other handlers on these signals exist.
   */
  virtual void RegisterUserDebugSignalHandlers(int signum = SIGUSR2) = 0;

  /**
   * Registers probes defined inside a tracing program.
   */
  virtual void RegisterTracepoint(
      sole::uuid trace_id,
      std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program) = 0;

  /**
   * Returns the status of the probe registration for the trace identified by the input ID.
   */
  virtual StatusOr<stirlingpb::Publish> GetTracepointInfo(sole::uuid trace_id) = 0;

  /**
   * Remove a dynamically created tracepoint.
   */
  virtual Status RemoveTracepoint(sole::uuid trace_id) = 0;

  /**
   * Populate the Publish Proto object. Agent calls this function to get the Publish
   * proto message. The proto publish message contains information (InfoClassSchema) on
   * all the Source Connectors that can be run to gather data and information on the types
   * for the data. The agent can then subscribe to a subset of the published message. The proto
   * is defined in //src/stirling/proto/stirling.proto.
   *
   */
  virtual void GetPublishProto(stirlingpb::Publish* publish_pb) = 0;

  /**
   * Register call-back from Agent. Used to periodically send data.
   *
   * Function signature is:
   *   uint64_t table_id
   *   std::unique_ptr<ColumnWrapperRecordBatch> data
   */
  virtual void RegisterDataPushCallback(DataPushCallback f) = 0;

  /**
   * Register a callback from the agent to fetch the latest metadata state.
   * This state is returned is constant and valid for the duration of the shared_ptr
   * lifetime. It should be periodically refreshed to update the metadata and release old
   * versions of the metadata to allow deletion of stale state.
   */
  virtual void RegisterAgentMetadataCallback(AgentMetadataCallback f) = 0;

  /**
   * Main data collection call. This version blocks, so make sure to wrap a thread around it.
   */
  virtual void Run() = 0;

  /**
   * Main data collection call. This version spawns off as an independent thread.
   */
  virtual Status RunAsThread() = 0;

  /**
   * Checks whether Stirling is running.
   */
  virtual bool IsRunning() const = 0;

  /**
   * Blocks until Stirling has initialized, and spawned off its main thread.
   * @param timeout Maximum time to wait until reaching running state.
   * @return error if doesn't reach running state within the time limit.
   */
  virtual Status WaitUntilRunning(std::chrono::milliseconds timeout) const = 0;

  /**
   * Wait for the running thread to terminate. Assumes previous call to RunThread().
   */
  virtual void WaitForThreadJoin() = 0;

  /**
   * Stop Stirling data collection, and perform any final clean-up actions.
   * Blocking, so will only return once the main loop has stopped.
   *
   * If Stirling is managing the thread, it will wait for thread to exit.
   * If external agent is managing the thread, it will wait until the main loop has exited.
   *
   * Note: this should be called in the case of a signal (e.g. SIGINT, SIGTERM, etc.)
   * to clean-up BPF deployed resources.
   */
  virtual void Stop() = 0;
};

namespace stirlingpb {
// This enables the GMOCK matcher to print out the Publish proto.
inline std::ostream& operator<<(std::ostream& os, const Publish& pub) {
  os << pub.DebugString();
  return os;
}

}  // namespace stirlingpb
}  // namespace stirling
}  // namespace px
