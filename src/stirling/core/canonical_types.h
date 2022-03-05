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
#include <string_view>

#include "src/stirling/core/types.h"

namespace px {
namespace stirling {
namespace canonical_data_elements {

// clang-format off

constexpr std::string_view kTimeColName = "time_";
constexpr DataElement kTime = {
    kTimeColName,
    "Timestamp when the data record was collected.",
    types::DataType::TIME64NS,
    types::SemanticType::ST_NONE,
    types::PatternType::METRIC_COUNTER};

constexpr std::string_view kUPIDColName = "upid";
constexpr DataElement kUPID = {
    kUPIDColName,
    "An opaque numeric ID that globally identify a running process inside the cluster.",
    types::DataType::UINT128,
    types::SemanticType::ST_UPID,
    types::PatternType::GENERAL};

// clang-format on

}  // namespace canonical_data_elements
}  // namespace stirling
}  // namespace px
