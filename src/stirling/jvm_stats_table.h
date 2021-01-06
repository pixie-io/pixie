#pragma once

#include "src/stirling/core/canonical_types.h"

namespace pl {
namespace stirling {

constexpr char kTime[] = "time_";
constexpr char kUPID[] = "upid";
constexpr char kYoungGCTime[] = "young_gc_time";
constexpr char kFullGCTime[] = "full_gc_time";
constexpr char kUsedHeapSize[] = "used_heap_size";
constexpr char kTotalHeapSize[] = "total_heap_size";
constexpr char kMaxHeapSize[] = "max_heap_size";

// clang-format off
constexpr DataElement kJVMStatsElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        {kYoungGCTime, "Young generation garbage collection time in nanoseconds.",
         types::DataType::INT64,
         types::SemanticType::ST_DURATION_NS,
         types::PatternType::METRIC_COUNTER},
        {kFullGCTime, "Full garbage collection time in nanoseconds.",
         types::DataType::INT64,
         types::SemanticType::ST_DURATION_NS,
         types::PatternType::METRIC_COUNTER},
        {kUsedHeapSize, "Used heap size in bytes.",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::METRIC_GAUGE},
        {kTotalHeapSize, "Total heap size in bytes.",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::METRIC_GAUGE},
        {kMaxHeapSize, "Maximal heap capacity in bytes.",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::METRIC_GAUGE},
};
// clang-format on

constexpr DataTableSchema kJVMStatsTable("jvm_stats", kJVMStatsElements,
                                         std::chrono::milliseconds{1000},
                                         std::chrono::milliseconds{1000});

#define DEFINE_IDX(col_name) constexpr int col_name##Idx = kJVMStatsTable.ColIndex(col_name)

DEFINE_IDX(kTime);
DEFINE_IDX(kUPID);
DEFINE_IDX(kYoungGCTime);
DEFINE_IDX(kFullGCTime);
DEFINE_IDX(kUsedHeapSize);
DEFINE_IDX(kTotalHeapSize);
DEFINE_IDX(kMaxHeapSize);

}  // namespace stirling
}  // namespace pl
