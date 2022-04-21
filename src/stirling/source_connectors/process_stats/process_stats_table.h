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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/output.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kProcessStatsElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        {"major_faults", "Number of major page faults",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"minor_faults", "Number of minor page faults",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"cpu_utime_ns", "Time spent on user space by the process",
         types::DataType::INT64, types::SemanticType::ST_DURATION_NS,
         types::PatternType::METRIC_COUNTER},
        {"cpu_ktime_ns", "Time spent on kernel by the process",
         types::DataType::INT64, types::SemanticType::ST_DURATION_NS,
         types::PatternType::METRIC_COUNTER},
        {"num_threads", "Number of threads of the process",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
        {"vsize_bytes", "Virtual memory size in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_GAUGE},
        {"rss_bytes", "Resident memory size in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_GAUGE},
        {"rchar_bytes", "IO reads in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"wchar_bytes", "IO writes in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"read_bytes", "IO reads actually go to storage layer in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"write_bytes", "IO writes actually go to storage layer in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
};

constexpr DataTableSchema kProcessStatsTable(
    "process_stats",
    "CPU, memory and IO stats for all K8s processes in your cluster.",
    kProcessStatsElements
);
// clang-format on
DEFINE_PRINT_TABLE(ProcessStats)

// TODO(oazizi): Enable version below, once rest of the agent supports tabletization.
//               Can't enable yet because it would result in time-scrambling.
//  static constexpr std::string_view kProcessStatsTabletizationKey = "upid";
//  static constexpr auto kProcessStatsTable =
//      DataTableSchema("process_stats", "CPU, memory and IO metrics for processes",
//      kProcessStatsElements, kProcessStatsTabletizationKey);

}  // namespace stirling
}  // namespace px
