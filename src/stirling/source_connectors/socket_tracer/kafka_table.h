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
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"

namespace px {
namespace stirling {

static const std::map<int64_t, std::string_view> kKafkaAPIKeyDecoder =
    px::EnumDefToMap<protocols::kafka::APIKey>();

// clang-format off
static constexpr DataElement kKafkaElements[] = {
      canonical_data_elements::kTime,
      canonical_data_elements::kUPID,
      canonical_data_elements::kRemoteAddr,
      canonical_data_elements::kRemotePort,
      canonical_data_elements::kLocalAddr,
      canonical_data_elements::kLocalPort,
      canonical_data_elements::kTraceRole,
      {"req_cmd", "Kafka request command",
       types::DataType::INT64,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL_ENUM,
       &kKafkaAPIKeyDecoder},
      {"client_id", "Kafka client ID",
       types::DataType::STRING,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"req_body", "Kafka request body",
       types::DataType::STRING,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"resp", "Kafka response",
       types::DataType::STRING,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
       canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
       canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kKafkaTable =
    DataTableSchema("kafka_events.beta", "Kafka resquest-response pair events", kKafkaElements);
DEFINE_PRINT_TABLE(Kafka)

constexpr int kKafkaTimeIdx = kKafkaTable.ColIndex("time_");
constexpr int kKafkaUPIDIdx = kKafkaTable.ColIndex("upid");
constexpr int kKafkaReqCmdIdx = kKafkaTable.ColIndex("req_cmd");
constexpr int kKafkaClientIDIdx = kKafkaTable.ColIndex("client_id");
constexpr int kKafkaReqBodyIdx = kKafkaTable.ColIndex("req_body");
constexpr int kKafkaRespIdx = kKafkaTable.ColIndex("resp");
constexpr int kKafkaLatencyIdx = kKafkaTable.ColIndex("latency");
#ifndef NDEBUG
constexpr int kKafkaPXInfoIdx = kKafkaTable.ColIndex("px_info_");
#endif
}  // namespace stirling
}  // namespace px
