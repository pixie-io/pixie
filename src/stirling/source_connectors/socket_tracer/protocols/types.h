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

#include <deque>
#include <variant>

#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types_gen.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/nats/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/sink/types.h"

namespace px {
namespace stirling {
namespace protocols {

// clang-format off
// PROTOCOL_LIST: Requires update on new protocols.
using FrameDequeVariant = std::variant<std::monostate,
                                       std::deque<cass::Frame>,
                                       std::deque<http::Message>,
                                       std::deque<mux::Frame>,
                                       std::deque<mysql::Packet>,
                                       std::deque<pgsql::RegularMessage>,
                                       std::deque<dns::Frame>,
                                       std::deque<redis::Message>,
                                       std::deque<kafka::Packet>,
                                       std::deque<nats::Message>,
                                       std::deque<amqp::Frame>,
                                       std::deque<sink::Message>>;
// clang-format off

}  // namespace protocols
}  // namespace stirling
}  // namespace px
