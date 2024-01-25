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
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types_gen.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kAMQPElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {
        "frame_type", "AMQP request command",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
        },
        {
        "channel", "AMQP channel",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
        },
        {"req_class_id", "AMQP request for request class_id",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
        },
        {"req_method_id", "AMQP request for request method_id",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
        },
        {"resp_class_id", "AMQP request for request class_id",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
        },
        {"resp_method_id", "AMQP request for request method_id",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
        },
        {"req_msg", "AMQP message req",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_msg", "AMQP message resp",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        canonical_data_elements::kLatencyNS,
};
// clang-format on

static constexpr auto kAMQPTable =
    DataTableSchema("amqp_events", "AMQP table pair events", kAMQPElements);
DEFINE_PRINT_TABLE(AMQP)

constexpr int kAMQPTimeIdx = kAMQPTable.ColIndex("time_");
constexpr int kAMQPUUIDIdx = kAMQPTable.ColIndex("upid");
constexpr int kAMQPFrameTypeIdx = kAMQPTable.ColIndex("frame_type");
constexpr int kAMQPChannelIdx = kAMQPTable.ColIndex("channel");
constexpr int kAMQPLatencyIdx = kAMQPTable.ColIndex("latency");

constexpr int kAMQPReqClassIdx = kAMQPTable.ColIndex("req_class_id");
constexpr int kAMQPReqMethodIdx = kAMQPTable.ColIndex("req_method_id");

constexpr int kAMQPRespClassIdx = kAMQPTable.ColIndex("resp_class_id");
constexpr int kAMQPRespMethodIdx = kAMQPTable.ColIndex("resp_method_id");

}  // namespace stirling
}  // namespace px
