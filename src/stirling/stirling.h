#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/common/error.h"
#include "src/common/status.h"
#include "src/stirling/data_table.h"
#include "src/stirling/proto/collector_config.pb.h"
#include "src/stirling/pub_sub_manager.h"
#include "src/stirling/source_connector.h"
#include "src/stirling/source_registry.h"

namespace pl {
namespace stirling {

/**
 * @brief Convenience function to subscribe to all elements in all schemas of
 * a published proto message. This should actually be in an agent.
 * TODO(kgandhi): Move to agent or common utils for agent when available.
 *
 * @param publish_proto
 * @return stirlingpb::Subscribe
 */
stirlingpb::Subscribe SubscribeToAllElements(const stirlingpb::Publish& publish_proto);

/**
 * The data collector collects data from various different 'sources',
 * and makes them available via a structured API, where the data can then be used and queried as
 * needed (by Pixie or others). Its function is to unify various, disparate sources of data into a
 * common, structured data format.
 */
class Stirling {
 public:
  Stirling() = delete;
  explicit Stirling(std::unique_ptr<SourceRegistry> registry) : registry_(std::move(registry)) {
    config_ = std::make_unique<PubSubManager>(schemas_);
  }
  ~Stirling() = default;

  /**
   * @brief Create data source connectors from the registered sources.
   *
   * @return Status
   */
  Status CreateSourceConnectors();

  /**
   * @brief Get the Publish Proto object. Agent calls this function to get the Publish
   * proto message. The proto publish message contains information (InfoClassSchema) on
   * all the Source Connectors that can be run to gather data and information on the types
   * for the data. The agent can then subscribe to a subset of the published message. The proto
   * is defined in //src/stirling/proto/collector_config.proto.
   *
   * @return stirlingpb::Publish
   */
  stirlingpb::Publish GetPublishProto();

  /**
   * @brief Get the Subscription object. Receive a Subscribe proto message from the agent.
   * Update the schemas based on the subscription message. Generate the appropriate tables
   * that conform to subscription information.
   *
   * @param subscribe_proto
   * @return Status
   */
  Status SetSubscription(const stirlingpb::Subscribe& subscribe_proto);

  /**
   * @brief Register call-back from Agent. Used to periodically send data.
   *
   * Function signature is:
   *   uint64_t table_id
   *   std::unique_ptr<ColumnWrapperRecordBatch> data
   */
  void RegisterCallback(
      std::function<void(uint64_t, std::unique_ptr<ColumnWrapperRecordBatch>)> f) {
    agent_callback_ = f;
  }

  // TODO(oazizi): Get rid of this eventually?
  /**
   * @brief Return a map of table ID to schema names.
   */
  std::unordered_map<uint64_t, std::string> TableIDToNameMap() {
    std::unordered_map<uint64_t, std::string> map;

    for (auto& schema : schemas_) {
      map.insert({schema->id(), schema->name()});
    }

    return map;
  }

  /**
   * Main data collection call, that is spawned off as an independent thread.
   */
  void Run();

  /**
   * Wait the running thread to terminate.
   */
  void Wait();

 private:
  /**
   * Main data source polling loop.
   */
  void RunThread();

  /**
   * Main data source polling loop.
   */
  Status AddSource(const std::string& name, std::unique_ptr<SourceConnector> source);

  /**
   * Helper function to figure out how much to sleep between polling iterations.
   */
  void SleepUntilNextTick();

  /**
   * Main thread used to spawn off RunThread().
   */
  std::thread run_thread_;

  /**
   * Vector of all Source Connectors.
   */
  std::vector<std::unique_ptr<SourceConnector>> sources_;

  /**
   * Vector of all Data Tables.
   */
  std::vector<std::unique_ptr<DataTable>> tables_;

  /**
   * Vector of all the Schemas.
   */
  std::vector<std::unique_ptr<InfoClassSchema>> schemas_;

  /**
   * Pointer the config unit that handles sub/pub with agent.
   */
  std::unique_ptr<PubSubManager> config_;

  /**
   * @brief Pointer to data source registry
   *
   */
  std::unique_ptr<SourceRegistry> registry_;

  // Defining a constant for data collector wrapper testing.
  const std::chrono::milliseconds kDefaultSamplingPeriod{100};

  /**
   * Function to call to push data to the agent.
   * Function signature is:
   *   uint64_t table_id
   *   std::unique_ptr<ColumnWrapperRecordBatch> data
   */
  std::function<void(uint64_t, std::unique_ptr<ColumnWrapperRecordBatch>)> agent_callback_;
};

}  // namespace stirling
}  // namespace pl
