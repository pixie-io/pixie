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
#include <map>
#include <memory>

#include "src/common/base/statusor.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"
#include "src/stirling/utils/parse_state.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

/**
 * Handlers are helper functions that transform MySQL Packets into request/response object.
 * MySQL Response can have one or more packets, so the functions pop off packets from the
 * deque as it parses the first packet.
 */
StatusOr<ParseState> HandleNoResponse(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      Record* entry);

StatusOr<ParseState> HandleErrMessage(DequeView<Packet> resp_packets, Record* entry);

StatusOr<ParseState> HandleOKMessage(DequeView<Packet> resp_packets, Record* entry);

/**
 * A Resultset can either be a binary resultset(returned by StmtExecute), or a text
 * resultset(returned by Query).
 */
StatusOr<ParseState> HandleResultsetResponse(DequeView<Packet> resp_packets, Record* entry,
                                             bool binaryresultset, bool multiresultset = false);

StatusOr<ParseState> HandleStmtPrepareOKResponse(DequeView<Packet> resp_packets, State* state,
                                                 Record* entry);

/**
 * MySQL Request can only have one packet, but StmtExecuteRequest is special. It needs to
 * look up the previously parsed StmtPrepare event based on a stmt_id when parsing the request.
 */
StatusOr<ParseState> HandleStmtExecuteRequest(const Packet& req_packet,
                                              std::map<int, PreparedStatement>* prepare_map,
                                              Record* entry);

/**
 * StmtClose request contains the stmt_id of the prepare stmt to close. It simply deletes
 * the prepare stmt from the map (state of ConnTracker).
 */
StatusOr<ParseState> HandleStmtCloseRequest(const Packet& req_packet,
                                            std::map<int, PreparedStatement>* prepare_map,
                                            Record* entry);

/**
 * Many requests have just a single string request (e.g. COM_QUERY).
 */
StatusOr<ParseState> HandleStringRequest(const Packet& req_packet, Record* entry);

/**
 * Some requests have more complex request bodies. Lump them in here for now.
 */
StatusOr<ParseState> HandleNonStringRequest(const Packet& req_packet, Record* entry);

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
