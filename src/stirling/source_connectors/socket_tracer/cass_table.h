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
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"

namespace px {
namespace stirling {

static const std::map<int64_t, std::string_view> kCQLReqOpDecoder =
    px::EnumDefToMap<protocols::cass::RespOp>();
static const std::map<int64_t, std::string_view> kCQLRespOpDecoder =
    px::EnumDefToMap<protocols::cass::RespOp>();

// clang-format off
static constexpr DataElement kCQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"req_op", "Request opcode",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kCQLReqOpDecoder},
        {"req_body", "Request body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_op", "Response opcode",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kCQLRespOpDecoder},
        {"resp_body", "Request body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kCQLTable =
    DataTableSchema("cql_events", "Cassandra (CQL) request-response pair events", kCQLElements);
DEFINE_PRINT_TABLE(CQL)

static constexpr int kCQLTraceRoleIdx = kCQLTable.ColIndex("trace_role");
static constexpr int kCQLUPIDIdx = kCQLTable.ColIndex("upid");
static constexpr int kCQLReqOp = kCQLTable.ColIndex("req_op");
static constexpr int kCQLReqBody = kCQLTable.ColIndex("req_body");
static constexpr int kCQLRespOp = kCQLTable.ColIndex("resp_op");
static constexpr int kCQLRespBody = kCQLTable.ColIndex("resp_body");
static constexpr int kCQLLatency = kCQLTable.ColIndex("latency");

}  // namespace stirling
}  // namespace px
