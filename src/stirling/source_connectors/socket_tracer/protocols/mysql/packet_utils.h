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

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

/**
 * The following functions check whether a Packet is of a certain type.
 */
bool IsEOFPacket(const Packet& packet);
bool IsErrPacket(const Packet& packet);
bool IsOKPacket(const Packet& packet);
bool IsStmtPrepareOKPacket(const Packet& packet);

/**
 * The following functions process packets by attempting to parse through the fields and check
 * there's nothing extra. The Process[Text/Binary]ResultsetRowPacket functions currently don't
 * return the processed resultset rows. However, they are named "Process" for consistency and
 * because , in the future, we may want to return processed resultset rows and append to table.
 */
// https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-ProtocolText::Resultset
Status ProcessTextResultsetRowPacket(const Packet& packet, size_t num_col);
// https://dev.mysql.com/doc/internals/en/binary-protocol-resultset-row.html
Status ProcessBinaryResultsetRowPacket(const Packet& packet, VectorView<ColDefinition> column_defs);
StatusOr<ColDefinition> ProcessColumnDefPacket(const Packet& packet);

/**
 * Checks an OK packet for the SERVER_MORE_RESULTS_EXISTS flag.
 */
bool MoreResultsExist(const Packet& last_packet);

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
