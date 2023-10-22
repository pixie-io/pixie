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

namespace px {
namespace stirling {

// clang-format off
constexpr DataElement kMQTTElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"req_control_packet_type", "Type Of the request MQTT Control Packet",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"req_header_fields", "MQTT Header Fields in request packet",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"req_properties", "MQTT properties in request packet",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"req_payload", "Payload of request packet",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_control_packet_type", "Type Of the response MQTT Control Packet",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_header_fields", "MQTT Header Fields in response packet",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_properties", "MQTT properties in response packet",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_payload", "Payload of response packet",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

constexpr auto kMQTTTable =
    DataTableSchema("mqtt_events", "MQTT request-response pair events", kMQTTElements);
DEFINE_PRINT_TABLE(MQTT)

constexpr int kMQTTTimeIdx = kMQTTTable.ColIndex("time_");
constexpr int kMQTTUPIDIdx = kMQTTTable.ColIndex("upid");
constexpr int kMQTTRemoteAddrIdx = kMQTTTable.ColIndex("remote_addr");
constexpr int kMQTTRemotePortIdx = kMQTTTable.ColIndex("remote_port");
constexpr int kMQTTTraceRoleIdx = kMQTTTable.ColIndex("trace_role");
constexpr int kMQTTReqControlTypeIdx = kMQTTTable.ColIndex("req_control_packet_type");
constexpr int kMQTTReqHeaderFieldsIdx = kMQTTTable.ColIndex("req_header_fields");
constexpr int kMQTTReqPropertiesIdx = kMQTTTable.ColIndex("req_properties");
constexpr int kMQTTReqPayloadIdx = kMQTTTable.ColIndex("req_payload");
constexpr int kMQTTRespControlTypeIdx = kMQTTTable.ColIndex("resp_control_packet_type");
constexpr int kMQTTRespHeaderFieldsIdx = kMQTTTable.ColIndex("resp_header_fields");
constexpr int kMQTTRespPropertiesIdx = kMQTTTable.ColIndex("resp_properties");
constexpr int kMQTTRespPayloadIdx = kMQTTTable.ColIndex("resp_payload");

}  // namespace stirling
}  // namespace px
