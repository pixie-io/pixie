#include <deque>
#include <memory>
#include <string>
#include <utility>

#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/mysql_handler.h"
#include "src/stirling/mysql/mysql_stitcher.h"

namespace pl {
namespace stirling {
namespace mysql {

namespace {
std::string CombinePrepareExecute(const StmtExecuteRequest* req,
                                  std::map<int, ReqRespEvent>* prepare_events) {
  auto iter = prepare_events->find(req->stmt_id());
  if (iter == prepare_events->end()) {
    LOG(WARNING) << absl::Substitute("Could not find prepare statement for stmt_id=$0",
                                     req->stmt_id());
    return "";
  }

  std::string_view stmt_prepare_request =
      static_cast<StringRequest*>(iter->second.request())->msg();

  size_t offset = 0;
  size_t count = 0;
  std::string result;

  for (size_t index = stmt_prepare_request.find("?", offset); index != std::string::npos;
       index = stmt_prepare_request.find("?", offset)) {
    if (count >= req->params().size()) {
      LOG(WARNING) << "Unequal number of stmt exec parameters for stmt prepare.";
      break;
    }
    absl::StrAppend(&result, stmt_prepare_request.substr(offset, index - offset),
                    req->params()[count].value);
    count++;
    offset = index + 1;
  }
  result += stmt_prepare_request.substr(offset);

  return result;
}

}  // namespace

// This function looks for unsynchronized req/resp packet queues.
// This could happen for a number of reasons:
//  - lost events
//  - previous unhandled case resulting in a bad state.
// Currently handles the case where an apparently missing request has left dangling responses,
// in which case those requests are popped off.
// TODO(oazizi): Also handle cases where responses should match to a later request (in which case
// requests should be popped off).
// TODO(oazizi): Should also consider sequence IDs in this function.
void SyncRespQueue(const Packet& req_packet, std::deque<Packet>* resp_packets) {
  // This handles the case where there are responses that pre-date a request.
  while (!resp_packets->empty()) {
    Packet& resp_packet = resp_packets->front();

    if (resp_packet.timestamp_ns > req_packet.timestamp_ns) {
      break;
    }

    LOG(WARNING) << absl::Substitute(
        "Ignoring response packet that pre-dates request. Size=$0 [OK=$1 ERR=$2 EOF=$3]",
        resp_packet.msg.size(), IsOKPacket(resp_packet), IsErrPacket(resp_packet),
        IsEOFPacket(resp_packet));
    resp_packets->pop_front();
  }
}

std::vector<Entry> ProcessMySQLPackets(std::deque<Packet>* req_packets,
                                       std::deque<Packet>* resp_packets, mysql::State* state) {
  std::vector<Entry> entries;

  // Process one request per loop iteration. Each request may consume 0, 1 or 2+ response packets.
  // The actual work is forked off to a helper function depending on the command type.
  // There are three possible outcomes for each request:
  //  1) Success. We continue to the next command.
  //  2) Needs more data: Not enough resp packets. We stop processing.
  //     We are still in a good state, and this is not considered an error.
  //  3) Error: An unexpected packet that indicates we have lost sync on the connection.
  //     This is communicated through the StatusOr mechanism.
  //     Recovery is the responsibility of the caller (i.e. ConnectionTracker).
  while (!req_packets->empty()) {
    Packet& req_packet = req_packets->front();

    // Command is the first byte.
    char command = req_packet.msg[0];

    VLOG(2) << absl::StrFormat("command=%x %s", command, req_packet.msg.substr(1));

    // For safety, make sure we have no stale response packets.
    SyncRespQueue(req_packet, resp_packets);

    // TODO(oazizi): Also try to sync if responses appear to be for the second request in the queue.
    // (i.e. dropped responses).

    StatusOr<ParseState> s;

    switch (DecodeCommand(command)) {
      // Internal commands with response: ERR_Packet.
      case MySQLEventType::kConnect:
      case MySQLEventType::kConnectOut:
      case MySQLEventType::kTime:
      case MySQLEventType::kDelayedInsert:
      case MySQLEventType::kDaemon:
        s = ProcessRequestWithBasicResponse(req_packet, resp_packets, &entries);
        break;

      // Basic Commands with response: OK_Packet or ERR_Packet
      case MySQLEventType::kSleep:
      case MySQLEventType::kInitDB:
      case MySQLEventType::kRegisterSlave:
      case MySQLEventType::kResetConnection:
      case MySQLEventType::kCreateDB:
      case MySQLEventType::kDropDB:
      case MySQLEventType::kProcessKill:
      case MySQLEventType::kRefresh:  // Deprecated.
      case MySQLEventType::kPing:     // COM_PING can't actually send ERR_Packet.
        s = ProcessRequestWithBasicResponse(req_packet, resp_packets, &entries);
        break;

      case MySQLEventType::kQuit:  // Response: OK_Packet or a connection close.
        s = ProcessRequestWithBasicResponse(req_packet, resp_packets, &entries);
        break;

      // Basic Commands with response: EOF_Packet or ERR_Packet.
      case MySQLEventType::kShutdown:  // Deprecated.
      case MySQLEventType::kSetOption:
      case MySQLEventType::kDebug:
        s = ProcessRequestWithBasicResponse(req_packet, resp_packets, &entries);
        break;

      // COM_FIELD_LIST has its own COM_FIELD_LIST meta response (ERR_Packet or one or more Column
      // Definition packets and a closing EOF_Packet).
      case MySQLEventType::kFieldList:  // Deprecated.
        s = ProcessFieldList(req_packet, resp_packets, &entries);
        break;

      // COM_QUERY has its own COM_QUERY meta response (ERR_Packet, OK_Packet,
      // Protocol::LOCAL_INFILE_Request, or ProtocolText::Resultset).
      case MySQLEventType::kQuery:
        s = ProcessQuery(req_packet, resp_packets, &entries);
        break;

      // COM_STMT_PREPARE returns COM_STMT_PREPARE_OK on success, ERR_Packet otherwise.
      case MySQLEventType::kStmtPrepare:
        s = ProcessStmtPrepare(req_packet, resp_packets, state, &entries);
        break;

      // COM_STMT_SEND_LONG_DATA has no response.
      case MySQLEventType::kStmtSendLongData:
        s = ProcessStmtSendLongData(req_packet, resp_packets, state, &entries);
        break;

      // COM_STMT_EXECUTE has its own COM_STMT_EXECUTE meta response (OK_Packet, ERR_Packet or a
      // resultset: Binary Protocol Resultset).
      case MySQLEventType::kStmtExecute:
        s = ProcessStmtExecute(req_packet, resp_packets, state, &entries);
        break;

      // COM_CLOSE has no response.
      case MySQLEventType::kStmtClose:
        s = ProcessStmtClose(req_packet, resp_packets, state, &entries);
        break;

      // COM_STMT_RESET response is OK_Packet if the statement could be reset, ERR_Packet if not.
      case MySQLEventType::kStmtReset:
        s = ProcessStmtReset(req_packet, resp_packets, state, &entries);
        break;

      // COM_STMT_FETCH has a meta response (multi-resultset, or ERR_Packet).
      case MySQLEventType::kStmtFetch:
        s = ProcessStmtFetch(req_packet, resp_packets, state, &entries);
        break;

      case MySQLEventType::kProcessInfo:     // a ProtocolText::Resultset or ERR_Packet
      case MySQLEventType::kChangeUser:      // Authentication Method Switch Request Packet or
                                             // ERR_Packet
      case MySQLEventType::kBinlogDumpGTID:  // binlog network stream, ERR_Packet or EOF_Packet
      case MySQLEventType::kBinlogDump:      // binlog network stream, ERR_Packet or EOF_Packet
      case MySQLEventType::kTableDump:       // a table dump or ERR_Packet
      case MySQLEventType::kStatistics:      // string.EOF
        // Rely on recovery to re-sync responses based on timestamps.
        s = error::Internal("Unimplemented command $0.", command);
        break;

      default:
        s = error::Internal("Unknown command $0.", command);
    }

    if (!s.ok()) {
      LOG(ERROR) << absl::Substitute("MySQL packet processing error: msg=$0", s.msg());
      LOG(ERROR) << "Will attempt response queue recovery.";

      // Just pop off the request, resync the response queue based on timestamps, and hope for the
      // best.
      req_packets->pop_front();
      SyncRespQueue(req_packet, resp_packets);
      continue;
    }

    ParseState result = s.ValueOrDie();
    DCHECK(result != ParseState::kInvalid);

    // Unlike an error, an unsuccessful iteration just means we don't have enough packets.
    // So stop processing and wait for more packets to arrive on next iteration.
    if (result == ParseState::kNeedsMoreData) {
      LOG_IF(WARNING, req_packets->size() != 1) << "Didn't have enough response packets, so would "
                                                   "have expected there to be only one request.";
      break;
    }

    req_packets->pop_front();
  }
  return entries;
}

StatusOr<ParseState> ProcessStmtPrepare(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                        mysql::State* state, std::vector<Entry>* entries) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  if (resp_packets->empty()) {
    // Need more data.
    return ParseState::kNeedsMoreData;
  }

  Packet& header_packet = resp_packets->front();
  if (IsErrPacket(header_packet)) {
    std::unique_ptr<ErrResponse> resp = HandleErrMessage(resp_packets);
    entries->push_back(Entry{MySQLEventType::kStmtPrepare, std::string(req->msg()),
                             MySQLRespStatus::kErr, std::string(resp->error_message()),
                             req_packet.timestamp_ns});
    return ParseState::kSuccess;
  }

  PL_ASSIGN_OR_RETURN(auto resp, HandleStmtPrepareOKResponse(resp_packets));
  int stmt_id = resp->resp_header().stmt_id;
  state->prepare_events.emplace(
      stmt_id, ReqRespEvent(MySQLEventType::kStmtPrepare, std::move(req), std::move(resp)));

  PL_UNUSED(entries);  // Don't push any entry for a prepare statement. Execute does that.

  return ParseState::kSuccess;
}

StatusOr<ParseState> ProcessStmtSendLongData(const Packet& /* req_packet */,
                                             std::deque<Packet>* resp_packets,
                                             mysql::State* /* state */,
                                             std::vector<Entry>* entries) {
  // COM_STMT_SEND_LONG_DATA doesn't have a response.
  PL_UNUSED(resp_packets);

  // No entries to record.
  PL_UNUSED(entries);

  return ParseState::kNeedsMoreData;
}

StatusOr<ParseState> ProcessStmtExecute(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                        mysql::State* state, std::vector<Entry>* entries) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStmtExecuteRequest(req_packet, &state->prepare_events));

  if (resp_packets->empty()) {
    // Need more data.
    return ParseState::kNeedsMoreData;
  }

  // TODO(chengruizhe/oazizi): Write result set to entry.
  std::string filled_msg = CombinePrepareExecute(req.get(), &state->prepare_events);

  Packet& first_resp_packet = resp_packets->front();

  if (IsErrPacket(first_resp_packet)) {
    std::unique_ptr<ErrResponse> resp = HandleErrMessage(resp_packets);
    entries->emplace_back(Entry{MySQLEventType::kStmtExecute, filled_msg, MySQLRespStatus::kErr,
                                std::string(resp->error_message()), req_packet.timestamp_ns});
    return ParseState::kSuccess;
  }

  if (IsOKPacket(first_resp_packet)) {
    HandleOKMessage(resp_packets);
  } else {
    // TODO(chengruizhe): Write result set to entry.
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));

    // Nullptr indicates there was not enough packets to process the Resultset.
    // This is not an error, but we return ParseState::kNeedsMoreData to indicate that we need more
    // data.
    if (resp == nullptr) {
      return ParseState::kNeedsMoreData;
    }
  }

  entries->emplace_back(Entry{MySQLEventType::kStmtExecute, filled_msg, MySQLRespStatus::kOK, "",
                              req_packet.timestamp_ns});
  return ParseState::kSuccess;
}

StatusOr<ParseState> ProcessStmtClose(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                      mysql::State* state, std::vector<Entry>* entries) {
  PL_RETURN_IF_ERROR(HandleStmtCloseRequest(req_packet, &state->prepare_events));

  // COM_STMT_CLOSE doesn't use any response packets.
  PL_UNUSED(resp_packets);

  // No entries to record.
  PL_UNUSED(entries);

  return ParseState::kSuccess;
}

StatusOr<ParseState> ProcessStmtFetch(const Packet& /* req_packet */,
                                      std::deque<Packet>* resp_packets, mysql::State* /* state */,
                                      std::vector<Entry>* /* entries */) {
  if (resp_packets->empty()) {
    // Need more data.
    return ParseState::kNeedsMoreData;
  }

  return error::Unimplemented("COM_STMT_FETCH is unhandled.");
}

StatusOr<ParseState> ProcessStmtReset(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                      mysql::State* state, std::vector<Entry>* entries) {
  PL_UNUSED(state);

  // Defer to basic response for now.
  return ProcessRequestWithBasicResponse(req_packet, resp_packets, entries);
}

StatusOr<ParseState> ProcessQuery(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                  std::vector<Entry>* entries) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  if (resp_packets->empty()) {
    // Need more data.
    return ParseState::kNeedsMoreData;
  }

  Packet& first_resp_packet = resp_packets->front();

  if (IsErrPacket(first_resp_packet)) {
    std::unique_ptr<ErrResponse> resp = HandleErrMessage(resp_packets);
    entries->emplace_back(Entry{MySQLEventType::kQuery, std::string(req->msg()),
                                MySQLRespStatus::kErr, std::string(resp->error_message()),
                                req_packet.timestamp_ns});
    return ParseState::kSuccess;
  }

  if (IsOKPacket(first_resp_packet)) {
    HandleOKMessage(resp_packets);
  } else {
    // TODO(chengruizhe): Write result set to entry.
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));

    // Nullptr indicates there was not enough packets to process the Resultset.
    // This is not an error, but we return ParseState::kNeedsMoreData to indicate that we need more
    // data.
    if (resp == nullptr) {
      return ParseState::kNeedsMoreData;
    }
  }

  entries->emplace_back(Entry{MySQLEventType::kQuery, std::string(req->msg()), MySQLRespStatus::kOK,
                              "", req_packet.timestamp_ns});
  return ParseState::kSuccess;
}

StatusOr<ParseState> ProcessFieldList(const Packet& /* req_packet */,
                                      std::deque<Packet>* resp_packets,
                                      std::vector<Entry>* /* entries */) {
  if (resp_packets->empty()) {
    // Need more data.
    return ParseState::kNeedsMoreData;
  }

  return error::Unimplemented("COM_FIELD_LIST is unhandled.");
}

StatusOr<ParseState> ProcessRequestWithBasicResponse(const Packet& /* req_packet */,
                                                     std::deque<Packet>* resp_packets,
                                                     std::vector<Entry>* entries) {
  if (resp_packets->empty()) {
    // Need more data.
    return ParseState::kNeedsMoreData;
  }

  Packet& resp_packet = resp_packets->front();
  if (!(IsOKPacket(resp_packet) || IsEOFPacket(resp_packet) || IsErrPacket(resp_packet))) {
    return error::Internal("Unexpected packet.");
  }
  resp_packets->pop_front();

  // Currently don't record basic request/response events.
  PL_UNUSED(entries);

  return ParseState::kSuccess;
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
