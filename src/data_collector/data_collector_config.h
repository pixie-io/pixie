#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "src/data_collector/info_class_schema.h"
#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

/*
 * Placeholder DataCollectorConfig class.
 * Primary functions are publish and subscribe.
 */
class DataCollectorConfig {
 public:
  DataCollectorConfig() = delete;
  ~DataCollectorConfig() = default;
  explicit DataCollectorConfig(const std::vector<std::unique_ptr<InfoClassSchema>>& schemas)
      : schemas_(schemas) {}

  /*
   * Generate a proto from the registered schemas, and publish the information upstream.
   * Return type will be a proto.
   */
  void Publish();

  /* On receiving a subscription proto, update the schemas.
   * Notify changes to top-level (which will trigger generation of tables, etc.).
   */
  void Subscribe();

 private:
  const std::vector<std::unique_ptr<InfoClassSchema>>& schemas_;
};

}  // namespace datacollector
}  // namespace pl
