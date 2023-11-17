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

#include "src/stirling/source_connectors/socket_tracer/conn_stats_table.h"

// PROTOCOL_LIST: Requires update on new protocols.
#include "src/stirling/source_connectors/socket_tracer/amqp_table.h"
#include "src/stirling/source_connectors/socket_tracer/cass_table.h"
#include "src/stirling/source_connectors/socket_tracer/dns_table.h"
#include "src/stirling/source_connectors/socket_tracer/http_table.h"
#include "src/stirling/source_connectors/socket_tracer/kafka_table.h"
#include "src/stirling/source_connectors/socket_tracer/mongodb_table.h"
#include "src/stirling/source_connectors/socket_tracer/mux_table.h"
#include "src/stirling/source_connectors/socket_tracer/mysql_table.h"
#include "src/stirling/source_connectors/socket_tracer/nats_table.h"
#include "src/stirling/source_connectors/socket_tracer/pgsql_table.h"
#include "src/stirling/source_connectors/socket_tracer/redis_table.h"
