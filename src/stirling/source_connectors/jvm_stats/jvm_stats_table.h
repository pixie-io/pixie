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

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/output.h"

namespace px {
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

constexpr DataTableSchema kJVMStatsTable(
        "jvm_stats",
        "Basic JVM memory management metrics for java processes. Includes information about "
        "memory use and garbage collection.",
        kJVMStatsElements
);
DEFINE_PRINT_TABLE(JVMStats);

// clang-format on

#define DEFINE_IDX(col_name) constexpr int col_name##Idx = kJVMStatsTable.ColIndex(col_name)

DEFINE_IDX(kTime);
DEFINE_IDX(kUPID);
DEFINE_IDX(kYoungGCTime);
DEFINE_IDX(kFullGCTime);
DEFINE_IDX(kUsedHeapSize);
DEFINE_IDX(kTotalHeapSize);
DEFINE_IDX(kMaxHeapSize);

}  // namespace stirling
}  // namespace px
