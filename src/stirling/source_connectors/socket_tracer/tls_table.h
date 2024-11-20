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
#include "src/stirling/source_connectors/socket_tracer/protocols/tls/types.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kTLSElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"req_type", "The type of request from the TLS record (Client/ServerHello, etc.)",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        {"version", "Version of TLS record",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        {"extensions", "Extensions in the TLS record",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kTLSTable =
    DataTableSchema("tls_events", "TLS request-response pair events", kTLSElements);
DEFINE_PRINT_TABLE(TLS)

constexpr int kTLSUPIDIdx = kTLSTable.ColIndex("upid");
constexpr int kTLSCmdIdx = kTLSTable.ColIndex("req_type");
constexpr int kTLSVersionIdx = kTLSTable.ColIndex("version");
constexpr int kTLSExtensionsIdx = kTLSTable.ColIndex("extensions");

}  // namespace stirling
}  // namespace px
