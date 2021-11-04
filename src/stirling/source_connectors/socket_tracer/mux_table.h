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
        canonical_data_elements::kTraceRole,
        /* {"req_header", "Request header", */
        /*  types::DataType::STRING, */
        /*  types::SemanticType::ST_NONE, */
        /*  types::PatternType::GENERAL}, */
        /* {"req_body", "Request", */
        /*  types::DataType::STRING, */
        /*  types::SemanticType::ST_NONE, */
        /*  types::PatternType::GENERAL}, */
        /* {"resp_header", "Response header", */
        /*  types::DataType::STRING, */
        /*  types::SemanticType::ST_NONE, */
        /*  types::PatternType::GENERAL}, */
        /* {"resp_body", "Response", */
        /*  types::DataType::STRING, */
        /*  types::SemanticType::ST_NONE, */
        /*  types::PatternType::GENERAL}, */
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kMuxTable =
    DataTableSchema("mux_events", "Mux request-response pair events", kMuxElements);
DEFINE_PRINT_TABLE(Mux)

static constexpr int kMuxUPIDIdx = kMuxTable.ColIndex("upid");
// TODO(ddelnano): Add these once the mux protocol is closer to working
/* static constexpr int kMuxReqHdrIdx = kMuxTable.ColIndex("req_header"); */
/* static constexpr int kMuxReqBodyIdx = kMuxTable.ColIndex("req_body"); */
/* static constexpr int kMuxRespHdrIdx = kMuxTable.ColIndex("resp_header"); */
/* static constexpr int kMuxRespBodyIdx = kMuxTable.ColIndex("resp_body"); */

}  // namespace stirling
}  // namespace px
