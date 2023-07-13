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

// PROTOCOL_LIST: Requires update on new protocols.
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/http/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/nats/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/stitcher.h"  // IWYU pragma: export
#include "src/stirling/source_connectors/socket_tracer/protocols/sink/stitcher.h"  // IWYU pragma: export
