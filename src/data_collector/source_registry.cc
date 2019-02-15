#include <functional>
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
 * @brief Create a Proc Stat Source
 *
 * @return std::unique_ptr<SourceConnector>
 */
std::unique_ptr<SourceConnector> CreateProcStatSource() {
  std::vector<InfoClassElement> elements = {
      InfoClassElement("_time", DataType::INT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
      InfoClassElement("system_percent", DataType::FLOAT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
      InfoClassElement("user_percent", DataType::FLOAT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
      InfoClassElement("idle_percent", DataType::FLOAT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED)};
  std::unique_ptr<SourceConnector> source_ptr =
      std::make_unique<ProcStatConnector>("proc_stat", elements);
  return source_ptr;
}

/**
 * @brief Register only metrics sources. All data sources that capture metrics
 * will be registered in this function. There will be similar functions for other
 * source types (logs and traces) in the future.
 * To exclude sources from certain builds, wrap the appropriate registration below
 * with ifdef statements and related build flags.
 *
 * @param registry
 */
void RegisterMetricsSources(SourceRegistry* registry) {
  CHECK(registry != nullptr);
  // Create a RegistryElement that has the source type and a function
  // to create a SourceConnector. Register source with name and RegistryElement
  // for an EBPF CPU Metrics source.
  SourceRegistry::RegistryElement ebpf_cpu_source_element(SourceType::kEBPF,
                                                          CreateEBPFCPUMetricsSource);
  registry->RegisterOrDie("ebpf_cpu_source", ebpf_cpu_source_element);

  // Register a proc stat data source.
  SourceRegistry::RegistryElement proc_stat_source_element(SourceType::kFile, CreateProcStatSource);
  registry->RegisterOrDie("proc_stat_source", proc_stat_source_element);

  // Can add all other metrics sources here.
  // We can also ifdef them out as needed: based on bazel configuration.
}

void RegisterSources(SourceRegistry* registry) {
  CHECK(registry != nullptr);
  RegisterMetricsSources(registry);
}

}  // namespace datacollector
}  // namespace pl
