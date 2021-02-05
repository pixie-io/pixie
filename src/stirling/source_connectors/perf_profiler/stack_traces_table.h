#pragma once

#include <map>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kElements[] = {
    canonical_data_elements::kTime,
    canonical_data_elements::kUPID,
    {"a", "A",
     types::DataType::FLOAT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
    {"b", "B",
     types::DataType::FLOAT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE}};
// clang-format on

constexpr auto kStackTraceTable =
    DataTableSchema("stack_traces", "Stack Traces", kElements, std::chrono::milliseconds{100},
                    std::chrono::milliseconds{1000});

}  // namespace stirling
}  // namespace pl
