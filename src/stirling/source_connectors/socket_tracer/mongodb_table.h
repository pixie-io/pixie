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
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/types.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kMongoDBElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"req_cmd", "MongoDB request command",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"req_body", "MongoDB request body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_status", "MongoDB response status",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_body", "MongoDB response body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
         canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kMongoDBTable =
    DataTableSchema("mongodb_events", "MongoDB request-response pair events", kMongoDBElements);
DEFINE_PRINT_TABLE(MongoDB)

constexpr int kMongoDBTimeIdx = kMongoDBTable.ColIndex("time_");
constexpr int kMongoDBUPIDIdx = kMongoDBTable.ColIndex("upid");
constexpr int kMongoDBReqCmdIdx = kMongoDBTable.ColIndex("req_cmd");
constexpr int kMongoDBReqBodyIdx = kMongoDBTable.ColIndex("req_body");
constexpr int kMongoDBRespStatusIdx = kMongoDBTable.ColIndex("resp_status");
constexpr int kMongoDBRespBodyIdx = kMongoDBTable.ColIndex("resp_body");
constexpr int kMongoDBLatencyIdx = kMongoDBTable.ColIndex("latency");

}  // namespace stirling
}  // namespace px
