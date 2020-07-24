#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <sole.hpp>

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/ir/logical.pb.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/source_registry.h"

namespace pl {
namespace stirling {

/**
 * Convenience function to subscribe to all info classes of
 * a published proto message. This should actually be in an agent.
 * TODO(kgandhi): Move to agent or common utils for agent when available.
 *
 * @param publish_proto
 * @return stirlingpb::Subscribe
 */
stirlingpb::Subscribe SubscribeToAllInfoClasses(const stirlingpb::Publish& publish_proto);

/**
 * Convenience function to subscribe to a single info classes of
 * a published proto message. This should actually be in an agent.
 * TODO(kgandhi): Move to agent or common utils for agent when available.
 *
 * @param publish_proto
 * @return stirlingpb::Subscribe
 */
stirlingpb::Subscribe SubscribeToInfoClass(const stirlingpb::Publish& publish_proto,
                                           std::string_view name);

/**
 * Specifies a set of sources to initialize as part of a source registry.
 */
enum class SourceRegistrySpecifier {
  // All available sources (including deprecated and test sources).
  kAll,

  // All production sources.
  kProd,

  // All production sources that generate metrics.
  kMetrics,

  // All production sources that are tracers.
  kTracers,
};

/**
 * Create a predefined source registry set. See SourceRegistrySpecifier for details.
 *
 * @param sources Specifier to indicate which pre-set source registry set is desired.
 * @return unique_ptr to the source registry containing all the sources to initialize.
 */
std::unique_ptr<SourceRegistry> CreateSourceRegistry(
    SourceRegistrySpecifier sources = SourceRegistrySpecifier::kProd);

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
   * Registers probes defined inside a tracing program.
   */
  virtual void RegisterTracepoint(
      sole::uuid trace_id, std::unique_ptr<dynamic_tracing::ir::logical::Program> program) = 0;

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
   * Get the Subscription object. Receive a Subscribe proto message from the agent.
   * Update the schemas based on the subscription message. Generate the appropriate tables
   * that conform to subscription information.
   *
   * @param subscribe_proto
   * @return Status
   */
  virtual Status SetSubscription(const stirlingpb::Subscribe& subscribe_proto) = 0;

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

}  // namespace stirling
}  // namespace pl
