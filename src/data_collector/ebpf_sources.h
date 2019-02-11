#pragma once

#include <memory>

#include "src/data_collector/source_connector.h"
#include "src/data_collector/source_registry.h"

namespace pl {
namespace datacollector {

void RegisterSourcesOrDie(SourceRegistry* registry);
void RegisterMetricsSourcesOrDie(SourceRegistry* registry);
std::unique_ptr<SourceConnector> CreateEBPFCPUMetricsSource();

}  // namespace datacollector
}  // namespace pl
