#include <memory>
#include <string>
#include <vector>

#include "src/data_collector/proto/collector_config.pb.h"
#include "src/data_collector/source_registry.h"

namespace pl {
namespace datacollector {

using datacollectorpb::Element_State;
using pl::types::DataType;

/**
 * @brief Register all data sources. Top level data collector is expected
 * to create the appropriate registry and then call this function. The top
 * level can determine the SourceRegistryType. So it could just pass a registry
 * for Metrics and only the supported metrics sources (EBPF, other sources as
 * controlled by ifdefs) would be registered.
 *
 * @param registry
 * @return Status
 */

void RegisterSourcesOrDie(SourceRegistry* registry) {
  CHECK(registry != nullptr);
  RegisterMetricsSourcesOrDie(registry);
}

/**
 * These are the steps to follow to add a new data source.
 * 1. Add a new function with the signature
 *    std::unique_ptr<SourceConnector> Creat{source_name}Source(). In this function:
 *    a. Create a schema (vector of InfoClassElement)
 *    b. For EBPF sources:
 *        i. Add the BPF source code and other parameters as needed.
 *        ii. Update BUILD.bazel pl_cc_resource target.
 *    c. For other sources: TBD.
 * 2. Register the data source in the appropriate Register function:
 *    i. RegisterMetricsSources
 *    ii. RegisterLogsSources
 *    iii. RegisterTracesSources
 */

// TODO(kgandhi): This function can be refactored in the future to be more generic
std::unique_ptr<SourceConnector> CreateEBPFCPUMetricsSource() {
  // EBPF CPU Data Source.
  // TODO(kgandhi): Coming in a future diff. Adding a bpf program to the end of an object file
  // currently only works on linux builds. We plan to add ifdefs around that to prevent breaking
  // the builds on other platforms.
  char prog = 0;  // There will be two extern chars pointing to locations in the obj file (marking
                  // start and end).
  char* bpf_prog_ptr = &prog;
  int bpf_prog_len = 0;
  const std::string bpf_program = std::string(bpf_prog_ptr, bpf_prog_len);

  // Create a vector of InfoClassElements.
  std::vector<InfoClassElement> elements = {
      InfoClassElement("_time", DataType::INT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
      InfoClassElement("cpu_id", DataType::INT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
      InfoClassElement("cpu_percentage", DataType::FLOAT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
  };

  std::unique_ptr<SourceConnector> source_ptr = std::make_unique<EBPFConnector>(
      "ebpf_cpu_metrics", elements, "finish_task_switch", "task_switch_event", bpf_program);

  return source_ptr;
}

/**
 * @brief Register only metrics sources. All data sources that capture metrics
 * will be registered in this function. There will be similar functions for other
 * source types (logs and traces) in the future.
 *
 * @param registry
 */
void RegisterMetricsSourcesOrDie(SourceRegistry* registry) {
  CHECK(registry != nullptr);
  registry->RegisterOrDie("ebpf_cpu_source", CreateEBPFCPUMetricsSource());
  // Can add all other metrics sources here.
  // We can also ifdef them out as needed: based on bazel configuration.
}

}  // namespace datacollector
}  // namespace pl
