#pragma once

#include "src/stirling/canonical_types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kJVMStatsElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        {"young_gc_time", types::DataType::DURATION64NS, types::PatternType::METRIC_COUNTER,
        "Young generation garbage collection time."},
        {"full_gc_time", types::DataType::DURATION64NS, types::PatternType::METRIC_COUNTER,
        "Full garbage collection time."},
        {"total_gc_time", types::DataType::DURATION64NS, types::PatternType::METRIC_COUNTER,
        "Total garbage collection time."},
        {"used_heap_size", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
         "Used heap size in bytes."},
        {"total_heap_size", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
         "Total heap size in bytes."},
        {"max_heap_size", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
         "Maximal heap capacity in bytes."},
};
// clang-format on

static constexpr auto kJVMStatsTable = DataTableSchema("jvm_stats", kJVMStatsElements);

}  // namespace stirling
}  // namespace pl
