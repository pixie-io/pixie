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

#include <absl/container/flat_hash_map.h>
#include <deque>
#include <variant>

#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types_gen.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/nats/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/types.h"

namespace px {
namespace stirling {
namespace protocols {

// clang-format off
// PROTOCOL_LIST: Requires update on new protocols.
// Note: stream_id is set to 0 for protocols that use a single stream / have no notion of streams.
using FrameDequeVariant = std::variant<std::monostate,
                                       absl::flat_hash_map<cass::stream_id_t, std::deque<cass::Frame>>,
                                       absl::flat_hash_map<http::stream_id_t, std::deque<http::Message>>,
                                       absl::flat_hash_map<mux::stream_id_t, std::deque<mux::Frame>>,
                                       absl::flat_hash_map<mysql::connection_id_t, std::deque<mysql::Packet>>,
                                       absl::flat_hash_map<pgsql::connection_id_t, std::deque<pgsql::RegularMessage>>,
                                       absl::flat_hash_map<dns::stream_id_t, std::deque<dns::Frame>>,
                                       absl::flat_hash_map<redis::stream_id_t, std::deque<redis::Message>>,
                                       absl::flat_hash_map<kafka::correlation_id_t, std::deque<kafka::Packet>>,
                                       absl::flat_hash_map<nats::stream_id_t, std::deque<nats::Message>>,
                                       absl::flat_hash_map<amqp::channel_id, std::deque<amqp::Frame>>,
                                       absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>>>;
// clang-format off

}  // namespace protocols
}  // namespace stirling
}  // namespace px
