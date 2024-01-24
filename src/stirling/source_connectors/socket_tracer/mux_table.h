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

#include "src/stirling/core/output.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/canonical_types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kMuxElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"req_type", "Mux message request type",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

// TODO(ddelnano): We may choose to augment the mux table with any
// nested thrift data during thriftmux (apache thrift over mux)
// or choose to store thrift data separately. stirling does not
// have support for handling nested protcols so this will require
// more discussion.
static constexpr auto kMuxTable =
    DataTableSchema("mux_events", "Mux request-response pair events", kMuxElements);
DEFINE_PRINT_TABLE(Mux)

static constexpr int kMuxUPIDIdx = kMuxTable.ColIndex("upid");
static constexpr int kMuxReqTypeIdx = kMuxTable.ColIndex("req_type");

}  // namespace stirling
}  // namespace px
