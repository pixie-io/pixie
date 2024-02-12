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

#include "src/stirling/core/output.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/canonical_types.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kPGSQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"req_cmd", "PostgreSQL request command code",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        {"req", "PostgreSQL request body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp", "PostgreSQL response body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kPGSQLTable = DataTableSchema(
    "pgsql_events", "Postgres (pgsql) request-response pair events", kPGSQLElements);
DEFINE_PRINT_TABLE(PGSQL)

constexpr int kPGSQLUPIDIdx = kPGSQLTable.ColIndex("upid");
constexpr int kPGSQLReqIdx = kPGSQLTable.ColIndex("req");
constexpr int kPGSQLRespIdx = kPGSQLTable.ColIndex("resp");
constexpr int kPGSQLReqCmdIdx = kPGSQLTable.ColIndex("req_cmd");
constexpr int kPGSQLLatencyIdx = kPGSQLTable.ColIndex("latency");

}  // namespace stirling
}  // namespace px
