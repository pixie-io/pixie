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

#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/stitcher.h"

#include <deque>
#include <string>
#include <utility>

#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/handler.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/packet_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

// This function looks for unsynchronized req/resp packet queues.
// This could happen for a number of reasons:
//  - lost events
//  - previous unhandled case resulting in a bad state.
// Currently handles the case where an apparently missing request has left dangling responses,
// in which case those requests are popped off.
void SyncRespQueue(const Packet& req_packet, std::deque<Packet>* resp_packets) {
  // This handles the case where there are responses that pre-date a request.
  while (!resp_packets->empty() && resp_packets->front().timestamp_ns < req_packet.timestamp_ns) {
    Packet& resp_packet = resp_packets->front();
    VLOG(1) << absl::Substitute(
        "Dropping response packet that pre-dates request. Size=$0 [OK=$1 ERR=$2 EOF=$3]",
        resp_packet.msg.size(), IsOKPacket(resp_packet), IsErrPacket(resp_packet),
        IsEOFPacket(resp_packet));
    resp_packets->pop_front();
  }
}

/**
 * Returns a read-only view of packets that correspond to the request packet at the head of
 * the request packets, which can then be sent for further processing as a contained bundle.
 *
 * The creation of the response packet bundle is done using timestamps and sequence numbers.
 * Any request with a timestamp that occurs after the timestamp of the 2nd request is not included.
 * Sequence numbers are also checked to be contiguous. Any gap results in sealing the bundle.
 *
 *
 * @param req_packets Deque of all received request packets (some may be missing).
 * @param resp_packets Dequeue of all received response packets (some may be missing).
 * @return View into the "bundle" of response packets that correspond to the first request packet.
 */
DequeView<Packet> GetRespView(const std::deque<Packet>& req_packets,
                              const std::deque<Packet>& resp_packets) {
  CTX_DCHECK(!req_packets.empty());

  int count = 0;

  for (const auto& resp_packet : resp_packets) {
    if (req_packets.size() > 1 && resp_packet.timestamp_ns > req_packets[1].timestamp_ns) {
      break;
    }

    uint8_t expected_seq_id = count + 1;
    if (resp_packet.sequence_id != expected_seq_id) {
      VLOG(1) << absl::Substitute(
          "Found packet with unexpected sequence ID [expected=$0 actual=$1]", expected_seq_id,
          resp_packet.sequence_id);
      break;
    }
    ++count;
  }

  return DequeView<Packet>(resp_packets, 0, count);
}

StatusOr<ParseState> ProcessPackets(const Packet& req_packet, DequeView<Packet> resp_packets_view,
                                    State* state, Record* entry) {
  char command_byte = req_packet.msg[0];
  Command command = DecodeCommand(command_byte);

  switch (command) {
    // Internal commands with response: ERR_Packet.
    case Command::kConnect:
    case Command::kConnectOut:
    case Command::kTime:
    case Command::kDelayedInsert:
    case Command::kDaemon:
      return ProcessRequestWithBasicResponse(req_packet, /* string_req */ false, resp_packets_view,
                                             entry);

    case Command::kInitDB:
    case Command::kCreateDB:
    case Command::kDropDB:
      return ProcessRequestWithBasicResponse(req_packet, /* string_req */ true, resp_packets_view,
                                             entry);

      // Basic Commands with response: OK_Packet or ERR_Packet
    case Command::kSleep:
    case Command::kRegisterSlave:
    case Command::kResetConnection:
    case Command::kProcessKill:
    case Command::kRefresh:  // Deprecated.
    case Command::kPing:     // COM_PING can't actually send ERR_Packet.
      return ProcessRequestWithBasicResponse(req_packet, /* string_req */ false, resp_packets_view,
                                             entry);

    case Command::kQuit:  // Response: OK_Packet or a connection close.
      return ProcessQuit(req_packet, resp_packets_view, entry);

      // Basic Commands with response: EOF_Packet or ERR_Packet.
    case Command::kShutdown:  // Deprecated.
    case Command::kSetOption:
    case Command::kDebug:
      return ProcessRequestWithBasicResponse(req_packet, /* string_req */ false, resp_packets_view,
                                             entry);

      // COM_FIELD_LIST has its own COM_FIELD_LIST meta response (ERR_Packet or one or more Column
      // Definition packets and a closing EOF_Packet).
    case Command::kFieldList:  // Deprecated.
      return ProcessFieldList(req_packet, resp_packets_view, entry);

      // COM_QUERY has its own COM_QUERY meta response (ERR_Packet, OK_Packet,
      // Protocol::LOCAL_INFILE_Request, or ProtocolText::Resultset).
    case Command::kQuery:
      return ProcessQuery(req_packet, resp_packets_view, entry);

      // COM_STMT_PREPARE returns COM_STMT_PREPARE_OK on success, ERR_Packet otherwise.
    case Command::kStmtPrepare:
      return ProcessStmtPrepare(req_packet, resp_packets_view, state, entry);

      // COM_STMT_SEND_LONG_DATA has no response.
    case Command::kStmtSendLongData:
      return ProcessStmtSendLongData(req_packet, resp_packets_view, state, entry);

      // COM_STMT_EXECUTE has its own COM_STMT_EXECUTE meta response (OK_Packet, ERR_Packet or a
      // resultset: Binary Protocol Resultset).
    case Command::kStmtExecute:
      return ProcessStmtExecute(req_packet, resp_packets_view, state, entry);

      // COM_CLOSE has no response.
    case Command::kStmtClose:
      return ProcessStmtClose(req_packet, resp_packets_view, state, entry);

      // COM_STMT_RESET response is OK_Packet if the statement could be reset, ERR_Packet if not.
    case Command::kStmtReset:
      return ProcessStmtReset(req_packet, resp_packets_view, state, entry);

      // COM_STMT_FETCH has a meta response (multi-resultset, or ERR_Packet).
    case Command::kStmtFetch:
      return ProcessStmtFetch(req_packet, resp_packets_view, state, entry);

    case Command::kProcessInfo:     // a ProtocolText::Resultset or ERR_Packet
    case Command::kChangeUser:      // Authentication Method Switch Request Packet or
                                    // ERR_Packet
    case Command::kBinlogDumpGTID:  // binlog network stream, ERR_Packet or EOF_Packet
    case Command::kBinlogDump:      // binlog network stream, ERR_Packet or EOF_Packet
    case Command::kTableDump:       // a table dump or ERR_Packet
    case Command::kStatistics:      // string.EOF
      // Rely on recovery to re-sync responses based on timestamps.
      return error::Internal("Unimplemented command $0.", magic_enum::enum_name(command));

    default:
      return error::Internal("Unknown command $0.", static_cast<int>(command));
  }
}

RecordsWithErrorCount<Record> ProcessMySQLPackets(std::deque<Packet>* req_packets,
                                                  std::deque<Packet>* resp_packets, State* state) {
  std::vector<Record> entries;
  int error_count = 0;

  // Process one request per loop iteration. Each request may consume 0, 1 or 2+ response packets.
  // The actual work is forked off to a helper function depending on the command type.
  // There are three possible outcomes for each request:
  //  1) Success. We continue to the next command.
  //  2) Needs more data: Not enough resp packets. We stop processing.
  //     We are still in a good state, and this is not considered an error.
  //  3) Error: An unexpected packet that indicates we have lost sync on the connection.
  //     This is communicated through the StatusOr mechanism.
  //     Recovery is the responsibility of the caller (i.e. ConnTracker).
  while (!req_packets->empty()) {
    Packet& req_packet = req_packets->front();

    // Command is the first byte.
    char command_byte = req_packet.msg[0];
    Command command = DecodeCommand(command_byte);

    VLOG(2) << absl::StrFormat("command=%x msg=%s", command_byte, req_packet.msg.substr(1));

    // For safety, make sure we have no stale response packets.
    SyncRespQueue(req_packet, resp_packets);

    DequeView<Packet> resp_packets_view = GetRespView(*req_packets, *resp_packets);

    VLOG(2) << absl::Substitute("req_packets=$0 resp_packets=$1 resp_view_size=$2",
                                req_packets->size(), resp_packets->size(),
                                resp_packets_view.size());

    // TODO(oazizi): Also try to sync if responses appear to be for the second request in the queue.
    // (i.e. dropped responses).

    Record entry;
    StatusOr<ParseState> s = ProcessPackets(req_packet, resp_packets_view, state, &entry);

    // This list contains the commands that, if parsed correctly,
    // are indicative of a higher confidence that this is indeed a MySQL protocol.
    if (!state->active && s.ok() && s.ValueOrDie() == ParseState::kSuccess) {
      switch (command) {
        case Command::kConnect:
        case Command::kInitDB:
        case Command::kCreateDB:
        case Command::kDropDB:
        case Command::kQuery:
        case Command::kStmtPrepare:
        case Command::kStmtExecute:
          state->active = true;
          break;
        default:
          break;
      }
    }

    if (!s.ok()) {
      VLOG(1) << absl::Substitute("MySQL packet processing error: msg=$0", s.msg());
      ++error_count;
    } else {
      ParseState result = s.ValueOrDie();
      CTX_DCHECK(result == ParseState::kSuccess || result == ParseState::kNeedsMoreData);

      if (result == ParseState::kNeedsMoreData) {
        bool is_last_req = req_packets->size() == 1;
        bool resp_looks_healthy = resp_packets_view.size() == resp_packets->size();
        if (is_last_req && resp_looks_healthy) {
          VLOG(3) << "Appears to be an incomplete message. Waiting for more data";
          // More response data will probably be captured in next iteration, so stop.
          break;
        }
        VLOG(1) << absl::Substitute(
            "Didn't have enough response packets, but doesn't appear to be partial either. "
            "[cmd=$0, cmd_msg=$1 resp_packets=$2]",
            magic_enum::enum_name(command), req_packet.msg.substr(1), resp_packets_view.size());
        ++error_count;
        // Continue on, since waiting for more packets likely won't help.
      } else {
        entries.push_back(std::move(entry));
      }
    }

    req_packets->pop_front();
    resp_packets->erase(resp_packets->begin(), resp_packets->begin() + resp_packets_view.size());
  }

  // If we haven't seen anything that gives us confidence that this is indeed a MySQL connection,
  // then don't return anything at all.
  if (!state->active) {
    return {{}, error_count};
  }

  return {entries, error_count};
}

#define PX_RETURN_IF_NOT_SUCCESS(stmt)       \
  {                                          \
    PX_ASSIGN_OR_RETURN(ParseState s, stmt); \
    if (s != ParseState::kSuccess) {         \
      return s;                              \
    }                                        \
  }

// Process a COM_STMT_PREPARE request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-stmt-prepare.html
StatusOr<ParseState> ProcessStmtPrepare(const Packet& req_packet, DequeView<Packet> resp_packets,
                                        State* state, Record* entry) {
  //----------------
  // Request
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleStringRequest(req_packet, entry));

  //----------------
  // Response
  //----------------

  if (resp_packets.empty()) {
    entry->resp.status = RespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  const Packet& first_resp_packet = resp_packets.front();

  if (IsErrPacket(first_resp_packet)) {
    PX_RETURN_IF_NOT_SUCCESS(HandleErrMessage(resp_packets, entry));

    return ParseState::kSuccess;
  }

  return HandleStmtPrepareOKResponse(resp_packets, state, entry);
}

// Process a COM_STMT_SEND_LONG_DATA request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-stmt-send-long-data.html
StatusOr<ParseState> ProcessStmtSendLongData(const Packet& req_packet,
                                             DequeView<Packet> resp_packets, State* /* state */,
                                             Record* entry) {
  //----------------
  // Request
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleNonStringRequest(req_packet, entry));

  //----------------
  // Response
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleNoResponse(req_packet, resp_packets, entry));

  return ParseState::kSuccess;
}

// Process a COM_STMT_EXECUTE request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-stmt-execute.html
StatusOr<ParseState> ProcessStmtExecute(const Packet& req_packet, DequeView<Packet> resp_packets,
                                        State* state, Record* entry) {
  //----------------
  // Request
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(
      HandleStmtExecuteRequest(req_packet, &state->prepared_statements, entry));

  //----------------
  // Response
  //----------------

  if (resp_packets.empty()) {
    entry->resp.status = RespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  const Packet& first_resp_packet = resp_packets.front();

  if (IsErrPacket(first_resp_packet)) {
    PX_RETURN_IF_NOT_SUCCESS(HandleErrMessage(resp_packets, entry));

    return ParseState::kSuccess;
  }

  if (IsOKPacket(first_resp_packet)) {
    PX_RETURN_IF_NOT_SUCCESS(HandleOKMessage(resp_packets, entry));

    return ParseState::kSuccess;
  }

  return HandleResultsetResponse(resp_packets, entry, /* binaryresultset */ true,
                                 /* multiresultset */ false);
}

// Process a COM_STMT_CLOSE request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-stmt-close.html
StatusOr<ParseState> ProcessStmtClose(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      State* state, Record* entry) {
  //----------------
  // Request
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleStmtCloseRequest(req_packet, &state->prepared_statements, entry));

  //----------------
  // Response
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleNoResponse(req_packet, resp_packets, entry));
  return ParseState::kSuccess;
}

// Process a COM_STMT_FETCH request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-stmt-fetch.html
StatusOr<ParseState> ProcessStmtFetch(const Packet& req_packet,
                                      DequeView<Packet> /* resp_packets */, State* /* state */,
                                      Record* entry) {
  //----------------
  // Request
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleNonStringRequest(req_packet, entry));

  //----------------
  // Response
  //----------------

  entry->resp.status = RespStatus::kUnknown;
  return error::Unimplemented("COM_STMT_FETCH response is unhandled.");
}

// Process a COM_STMT_RESET request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-stmt-reset.html
StatusOr<ParseState> ProcessStmtReset(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      State* state, Record* entry) {
  PX_UNUSED(state);

  // Defer to basic response for now.
  return ProcessRequestWithBasicResponse(req_packet, /* string_req */ false, resp_packets, entry);
}

// Process a COM_QUERY request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-query.html
StatusOr<ParseState> ProcessQuery(const Packet& req_packet, DequeView<Packet> resp_packets,
                                  Record* entry) {
  //----------------
  // Request
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleStringRequest(req_packet, entry));

  //----------------
  // Response
  //----------------

  if (resp_packets.empty()) {
    entry->resp.status = RespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  const Packet& first_resp_packet = resp_packets.front();

  if (IsErrPacket(first_resp_packet)) {
    PX_RETURN_IF_NOT_SUCCESS(HandleErrMessage(resp_packets, entry));

    return ParseState::kSuccess;
  }

  if (IsOKPacket(first_resp_packet)) {
    PX_RETURN_IF_NOT_SUCCESS(HandleOKMessage(resp_packets, entry));

    return ParseState::kSuccess;
  }

  return HandleResultsetResponse(resp_packets, entry, /* binaryresultset */ false,
                                 /* multiresultset */ false);
}

// Process a COM_FIELD_LIST request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-field-list.html
StatusOr<ParseState> ProcessFieldList(const Packet& req_packet,
                                      DequeView<Packet> /* resp_packets */, Record* entry) {
  //----------------
  // Request
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleStringRequest(req_packet, entry));

  //----------------
  // Response
  //----------------

  entry->resp.status = RespStatus::kUnknown;
  return error::Unimplemented("COM_FIELD_LIST response is unhandled.");
}

// Process a COM_QUIT request and response, and populate details into a record entry.
// MySQL documentation: https://dev.mysql.com/doc/internals/en/com-quit.html
StatusOr<ParseState> ProcessQuit(const Packet& req_packet, DequeView<Packet> resp_packets,
                                 Record* entry) {
  //----------------
  // Request
  //----------------

  PX_RETURN_IF_NOT_SUCCESS(HandleNonStringRequest(req_packet, entry));

  //----------------
  // Response
  //----------------

  if (resp_packets.empty()) {
    entry->resp.status = RespStatus::kNone;
    entry->resp.timestamp_ns = req_packet.timestamp_ns;
    return ParseState::kSuccess;
  }

  const Packet& resp_packet = resp_packets.front();
  if (IsOKPacket(resp_packet)) {
    entry->resp.status = RespStatus::kOK;
    entry->resp.timestamp_ns = resp_packet.timestamp_ns;
    return ParseState::kSuccess;
  }

  return error::Internal("Extra response packet after ComQuit.");
}

// Process a simple request and response pair, and populate details into a record entry.
// This is for MySQL commands that have only a single OK, ERR or EOF response.
// TODO(oazizi): Currently any of OK, ERR or EOF are accepted, but could specialize
// to expect a subset, since some responses are invalid for certain commands.
// For example, a COM_INIT_DB command should never receive an EOF response.
// All we would do is print a warning, though, so this is low priority.
StatusOr<ParseState> ProcessRequestWithBasicResponse(const Packet& req_packet, bool string_req,
                                                     DequeView<Packet> resp_packets,
                                                     Record* entry) {
  //----------------
  // Request
  //----------------

  if (string_req) {
    HandleStringRequest(req_packet, entry);
  } else {
    HandleNonStringRequest(req_packet, entry);
  }

  //----------------
  // Response
  //----------------

  if (resp_packets.empty()) {
    entry->resp.status = RespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  if (resp_packets.size() > 1) {
    return error::Internal(
        "Did not expect more than one response packet [cmd=$0, num_extra_packets=$1].",
        static_cast<uint8_t>(req_packet.msg[0]), resp_packets.size() - 1);
  }

  const Packet& resp_packet = resp_packets.front();

  if (IsOKPacket(resp_packet) || IsEOFPacket(resp_packet)) {
    entry->resp.status = RespStatus::kOK;
    entry->resp.timestamp_ns = resp_packet.timestamp_ns;
    return ParseState::kSuccess;
  }

  if (IsErrPacket(resp_packet)) {
    return HandleErrMessage(resp_packets, entry);
  }

  entry->resp.status = RespStatus::kUnknown;
  return error::Internal("Unexpected packet");
}

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
