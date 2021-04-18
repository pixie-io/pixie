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
#include <string>
#include <utility>
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {
namespace testutils {

/**
 * Generates the bytes of a length-encoded integer.
 * https://dev.mysql.com/doc/internals/en/integer.html#length-encoded-integer
 */
std::string LengthEncodedInt(int num);

/**
 * Generates the bytes of a length-encoded string, which consists of a
 * length-encoded-integer representing the size of the string, followed by the string contents.
 * https://dev.mysql.com/doc/internals/en/string.html
 */
std::string LengthEncodedString(std::string_view s);

std::string GenRawPacket(uint8_t packet_num, std::string_view msg);

std::string GenRawPacket(const Packet& packet);

std::string GenRequestPacket(Command command, std::string_view msg);

Packet GenCountPacket(uint8_t seq_id, int num_col);

Packet GenColDefinition(uint8_t seq_id, const ColDefinition& col_def);

Packet GenResultsetRow(uint8_t seq_id, const ResultsetRow& row);

Packet GenStmtPrepareRespHeader(uint8_t seq_id, const StmtPrepareRespHeader& header);

Packet GenStmtExecuteRequest(const StmtExecuteRequest& req);

Packet GenStmtCloseRequest(const StmtCloseRequest& req);

Packet GenStringRequest(const StringRequest& req, Command type);

Packet GenStringRequest(const StringRequest& req, char command);

std::deque<Packet> GenResultset(const Resultset& resultset, bool client_eof_deprecate = false);

std::deque<Packet> GenStmtPrepareOKResponse(const StmtPrepareOKResponse& resp);

Packet GenErr(uint8_t seq_id, const ErrResponse& err);

Packet GenOK(uint8_t seq_id);

Packet GenEOF(uint8_t seq_id);

}  // namespace testutils
}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
