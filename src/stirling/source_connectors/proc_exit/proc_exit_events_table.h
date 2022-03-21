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
#include "src/stirling/core/types.h"

namespace px {
namespace stirling {
namespace proc_exit_tracer {

constexpr std::string_view kProcExitsName = "proc_exit_events";
constexpr std::string_view kExitCodeColName = "exit_code";
constexpr std::string_view kSignalColName = "signal";
constexpr std::string_view kCommColName = "comm";

// clang-format off
constexpr DataElement kElements[] = {
    canonical_data_elements::kTime,
    canonical_data_elements::kUPID,
    {kExitCodeColName,
    "The exit code of this process.",
    types::DataType::INT64,
    types::SemanticType::ST_NONE,
    types::PatternType::GENERAL},
    {kSignalColName,
    "The signal received by this process.",
    types::DataType::INT64,
    types::SemanticType::ST_NONE,
    types::PatternType::GENERAL},
    {kCommColName,
    "The name of this process.",
    types::DataType::STRING,
    types::SemanticType::ST_NONE,
    types::PatternType::GENERAL},
};
// clang-format on

constexpr auto kProcExitEventsTable =
    DataTableSchema(kProcExitsName, "Traces all abnormal process exits", kElements);

constexpr int kTimeIdx = kProcExitEventsTable.ColIndex(canonical_data_elements::kTimeColName);
constexpr int kUPIDIdx = kProcExitEventsTable.ColIndex(canonical_data_elements::kUPIDColName);
constexpr int kExitCodeIdx = kProcExitEventsTable.ColIndex(kExitCodeColName);
constexpr int kSignalIdx = kProcExitEventsTable.ColIndex(kSignalColName);
constexpr int kCommIdx = kProcExitEventsTable.ColIndex(kCommColName);

}  // namespace proc_exit_tracer
}  // namespace stirling
}  // namespace px
