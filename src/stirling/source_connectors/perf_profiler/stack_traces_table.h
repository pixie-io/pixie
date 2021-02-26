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
    {"stack_trace_id",
     "A unique identifier of the stack trace, for script-writing convenience. "
     "String representation is in the `stack_trace` column.",
     types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
    {"stack_trace",
     "A stack trace within the sampled process, in folded format. "
     "The call stack symbols are separated by semicolons. "
     "If symbols cannot be resolved, addresses are populated instead.",
     types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
    {"count",
     "Number of times the stack trace has been sampled.",
     types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE}
};

constexpr auto kStackTraceTable = DataTableSchema(
        "stack_traces.beta",
        "Sampled stack traces of applications that identify hot-spots in application code. "
        "Executable symbols are required for human-readable function names to be displayed.",
        kElements,
        std::chrono::milliseconds{1000},
        std::chrono::milliseconds{5000}
);
// clang-format on

constexpr int kStackTraceTimeIdx = kStackTraceTable.ColIndex("time_");
constexpr int kStackTraceUPIDIdx = kStackTraceTable.ColIndex("upid");
constexpr int kStackTraceStackTraceIDIdx = kStackTraceTable.ColIndex("stack_trace_id");
constexpr int kStackTraceStackTraceStrIdx = kStackTraceTable.ColIndex("stack_trace");
constexpr int kStackTraceCountIdx = kStackTraceTable.ColIndex("count");

}  // namespace stirling
}  // namespace pl
