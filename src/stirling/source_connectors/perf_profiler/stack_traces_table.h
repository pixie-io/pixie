/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <map>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/output.h"
#include "src/stirling/core/types.h"

namespace px {
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
        kElements
);
// clang-format on
DEFINE_PRINT_TABLE(StackTrace)

constexpr int kStackTraceTimeIdx = kStackTraceTable.ColIndex("time_");
constexpr int kStackTraceUPIDIdx = kStackTraceTable.ColIndex("upid");
constexpr int kStackTraceStackTraceIDIdx = kStackTraceTable.ColIndex("stack_trace_id");
constexpr int kStackTraceStackTraceStrIdx = kStackTraceTable.ColIndex("stack_trace");
constexpr int kStackTraceCountIdx = kStackTraceTable.ColIndex("count");

}  // namespace stirling
}  // namespace px
