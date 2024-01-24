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
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {

static const std::map<int64_t, std::string_view> kMySQLReqCmdDecoder =
    px::EnumDefToMap<protocols::mysql::Command>();
static const std::map<int64_t, std::string_view> kMySQLRespStatusDecoder =
    px::EnumDefToMap<protocols::mysql::RespStatus>();

// clang-format off
static constexpr DataElement kMySQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"req_cmd", "MySQL request command",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kMySQLReqCmdDecoder},
        {"req_body", "MySQL request body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_status", "MySQL response status code",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kMySQLRespStatusDecoder},
        {"resp_body", "MySQL response body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kMySQLTable =
    DataTableSchema("mysql_events", "MySQL resquest-response pair events", kMySQLElements);
DEFINE_PRINT_TABLE(MySQL)

constexpr int kMySQLTimeIdx = kMySQLTable.ColIndex("time_");
constexpr int kMySQLUPIDIdx = kMySQLTable.ColIndex("upid");
constexpr int kMySQLReqCmdIdx = kMySQLTable.ColIndex("req_cmd");
constexpr int kMySQLReqBodyIdx = kMySQLTable.ColIndex("req_body");
constexpr int kMySQLRespStatusIdx = kMySQLTable.ColIndex("resp_status");
constexpr int kMySQLRespBodyIdx = kMySQLTable.ColIndex("resp_body");
constexpr int kMySQLLatencyIdx = kMySQLTable.ColIndex("latency");

}  // namespace stirling
}  // namespace px
