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
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kDNSElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"req_header", "Request header",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"req_body", "Request",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_header", "Response header",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_body", "Response",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kDNSTable =
    DataTableSchema("dns_events", "DNS request-response pair events", kDNSElements);
DEFINE_PRINT_TABLE(DNS)

static constexpr int kDNSUPIDIdx = kDNSTable.ColIndex("upid");
static constexpr int kDNSReqHdrIdx = kDNSTable.ColIndex("req_header");
static constexpr int kDNSReqBodyIdx = kDNSTable.ColIndex("req_body");
static constexpr int kDNSRespHdrIdx = kDNSTable.ColIndex("resp_header");
static constexpr int kDNSRespBodyIdx = kDNSTable.ColIndex("resp_body");

}  // namespace stirling
}  // namespace px
