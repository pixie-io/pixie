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

// TODO(chengruizhe): Use RapidJSON to generate JSON.
std::string CreateErrorJSON(std::string_view message, std::string_view error) {
  return absl::Substitute(R"({"Error": "$0", "Message": "$1"})", error, message);
}

std::string CreateMessageJSON(std::string_view message) {
  return absl::Substitute(R"({"Message": "$0"})", message);
}

}  // namespace

// A wrapper around PL_ASSIGN_OR_RETURN that adds the braces.
// Only works if lhs is not a declaration, but that is the case here.
#define PL_ASSIGN_OR_RETURN_NO_DECL(lhs, rexpr) \
  { PL_ASSIGN_OR_RETURN(lhs, rexpr); }

StatusOr<std::vector<Entry>> StitchMySQLPackets(std::deque<Packet>* req_packets,
                                                std::deque<Packet>* resp_packets,
                                                mysql::State* state) {
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

    StatusOr<bool> s;
    bool success = false;

    switch (DecodeEventType(command)) {
      // Internal commands with response: ERR_Packet.
      case MySQLEventType::kConnect:
      case MySQLEventType::kConnectOut:
      case MySQLEventType::kTime:
      case MySQLEventType::kDelayedInsert:
      case MySQLEventType::kDaemon:
        PL_ASSIGN_OR_RETURN_NO_DECL(
            success, StitchRequestWithBasicResponse(req_packet, resp_packets, &entries));
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
        PL_ASSIGN_OR_RETURN_NO_DECL(
            success, StitchRequestWithBasicResponse(req_packet, resp_packets, &entries));
        break;

      case MySQLEventType::kQuit:  // Response: OK_Packet or a connection close.
        PL_ASSIGN_OR_RETURN_NO_DECL(
            success, StitchRequestWithBasicResponse(req_packet, resp_packets, &entries));
        break;

      // Basic Commands with response: EOF_Packet or ERR_Packet.
      case MySQLEventType::kShutdown:  // Deprecated.
      case MySQLEventType::kSetOption:
      case MySQLEventType::kDebug:
        PL_ASSIGN_OR_RETURN_NO_DECL(
            success, StitchRequestWithBasicResponse(req_packet, resp_packets, &entries));
        break;

      // COM_FIELD_LIST has its own COM_FIELD_LIST meta response (ERR_Packet or one or more Column
      // Definition packets and a closing EOF_Packet).
      case MySQLEventType::kFieldList:  // Deprecated.
        PL_ASSIGN_OR_RETURN_NO_DECL(success, StitchFieldList(req_packet, resp_packets, &entries));
        break;

      // COM_QUERY has its own COM_QUERY meta response (ERR_Packet, OK_Packet,
      // Protocol::LOCAL_INFILE_Request, or ProtocolText::Resultset).
      case MySQLEventType::kQuery:
        PL_ASSIGN_OR_RETURN_NO_DECL(success, StitchQuery(req_packet, resp_packets, &entries));
        break;

      // COM_STMT_PREPARE returns COM_STMT_PREPARE_OK on success, ERR_Packet otherwise.
      case MySQLEventType::kStmtPrepare:
        PL_ASSIGN_OR_RETURN_NO_DECL(success,
                                    StitchStmtPrepare(req_packet, resp_packets, state, &entries));
        break;

      // COM_STMT_SEND_LONG_DATA has no response.
      case MySQLEventType::kStmtSendLongData:
        PL_ASSIGN_OR_RETURN_NO_DECL(
            success, StitchStmtSendLongData(req_packet, resp_packets, state, &entries));
        break;

      // COM_STMT_EXECUTE has its own COM_STMT_EXECUTE meta response (OK_Packet, ERR_Packet or a
      // resultset: Binary Protocol Resultset).
      case MySQLEventType::kStmtExecute:
        PL_ASSIGN_OR_RETURN_NO_DECL(success,
                                    StitchStmtExecute(req_packet, resp_packets, state, &entries));
        break;

      // COM_CLOSE has no response.
      case MySQLEventType::kStmtClose:
        PL_ASSIGN_OR_RETURN_NO_DECL(success,
                                    StitchStmtClose(req_packet, resp_packets, state, &entries));
        break;

      // COM_STMT_RESET response is OK_Packet if the statement could be reset, ERR_Packet if not.
      case MySQLEventType::kStmtReset:
        PL_ASSIGN_OR_RETURN_NO_DECL(success,
                                    StitchStmtReset(req_packet, resp_packets, state, &entries));
        break;

      // COM_STMT_FETCH has a meta response (multi-resultset, or ERR_Packet).
      case MySQLEventType::kStmtFetch:
        PL_ASSIGN_OR_RETURN_NO_DECL(success,
                                    StitchStmtFetch(req_packet, resp_packets, state, &entries));
        break;

      case MySQLEventType::kProcessInfo:     // a ProtocolText::Resultset or ERR_Packet
      case MySQLEventType::kChangeUser:      // Authentication Method Switch Request Packet or
                                             // ERR_Packet
      case MySQLEventType::kBinlogDumpGTID:  // binlog network stream, ERR_Packet or EOF_Packet
      case MySQLEventType::kBinlogDump:      // binlog network stream, ERR_Packet or EOF_Packet
      case MySQLEventType::kTableDump:       // a table dump or ERR_Packet
      case MySQLEventType::kStatistics:      // string.EOF
        // Rely on recovery to re-sync responses based on timestamps.
        return error::Internal("Unimplemented command $0.", command);

      default:
        return error::Internal("Unknown command $0.", command);
    }

    if (!success) {
      break;
    }

    req_packets->pop_front();
  }
  return entries;
}

StatusOr<bool> StitchStmtPrepare(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                 mysql::State* state, std::vector<Entry>* entries) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  if (resp_packets->empty()) {
    // Need more data.
    return false;
  }

  Packet& header_packet = resp_packets->front();
  if (IsErrPacket(header_packet)) {
    std::unique_ptr<ErrResponse> resp = HandleErrMessage(resp_packets);
    entries->push_back(Entry{CreateErrorJSON(req->msg(), resp->error_message()),
                             MySQLEntryStatus::kErr, req_packet.timestamp_ns});
    return true;
  }

  PL_ASSIGN_OR_RETURN(auto resp, HandleStmtPrepareOKResponse(resp_packets));
  int stmt_id = resp->resp_header().stmt_id;
  state->prepare_events.emplace(
      stmt_id, ReqRespEvent(MySQLEventType::kStmtPrepare, std::move(req), std::move(resp)));

  PL_UNUSED(entries);  // Don't push any entry for a prepare statement. Execute does that.

  return true;
}

StatusOr<bool> StitchStmtSendLongData(const Packet& /* req_packet */,
                                      std::deque<Packet>* resp_packets, mysql::State* /* state */,
                                      std::vector<Entry>* entries) {
  // COM_STMT_SEND_LONG_DATA doesn't have a response.
  PL_UNUSED(resp_packets);

  // No entries to record.
  PL_UNUSED(entries);

  return false;
}

StatusOr<bool> StitchStmtExecute(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                 mysql::State* state, std::vector<Entry>* entries) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStmtExecuteRequest(req_packet, &state->prepare_events));

  if (resp_packets->empty()) {
    // Need more data.
    return false;
  }

  // TODO(chengruizhe/oazizi): Write result set to entry.
  std::string filled_msg = CombinePrepareExecute(req.get(), &state->prepare_events);

  Packet& first_resp_packet = resp_packets->front();

  if (IsErrPacket(first_resp_packet)) {
    std::unique_ptr<ErrResponse> resp = HandleErrMessage(resp_packets);
    entries->emplace_back(Entry{CreateErrorJSON(filled_msg, resp->error_message()),
                                MySQLEntryStatus::kErr, req_packet.timestamp_ns});
    return true;
  }

  if (IsOKPacket(first_resp_packet)) {
    HandleOKMessage(resp_packets);
  } else {
    // TODO(chengruizhe): Write result set to entry.
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));

    // Nullptr indicates there was not enough packets to process the Resultset.
    // This is not an error, but we return false to indicate that we need more data.
    if (resp == nullptr) {
      return false;
    }
  }

  entries->emplace_back(
      Entry{CreateMessageJSON(filled_msg), MySQLEntryStatus::kOK, req_packet.timestamp_ns});
  return true;
}

StatusOr<bool> StitchStmtClose(const Packet& req_packet, std::deque<Packet>* resp_packets,
                               mysql::State* state, std::vector<Entry>* entries) {
  PL_RETURN_IF_ERROR(HandleStmtCloseRequest(req_packet, &state->prepare_events));

  // COM_STMT_CLOSE doesn't use any response packets.
  PL_UNUSED(resp_packets);

  // No entries to record.
  PL_UNUSED(entries);

  return true;
}

StatusOr<bool> StitchStmtFetch(const Packet& /* req_packet */, std::deque<Packet>* resp_packets,
                               mysql::State* /* state */, std::vector<Entry>* /* entries */) {
  if (resp_packets->empty()) {
    // Need more data.
    return false;
  }

  return error::Unimplemented("COM_STMT_FETCH is unhandled.");
}

StatusOr<bool> StitchStmtReset(const Packet& req_packet, std::deque<Packet>* resp_packets,
                               mysql::State* state, std::vector<Entry>* entries) {
  PL_UNUSED(state);

  // Defer to basic response for now.
  return StitchRequestWithBasicResponse(req_packet, resp_packets, entries);
}

StatusOr<bool> StitchQuery(const Packet& req_packet, std::deque<Packet>* resp_packets,
                           std::vector<Entry>* entries) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  if (resp_packets->empty()) {
    // Need more data.
    return false;
  }

  Packet& first_resp_packet = resp_packets->front();

  if (IsErrPacket(first_resp_packet)) {
    std::unique_ptr<ErrResponse> resp = HandleErrMessage(resp_packets);
    entries->emplace_back(Entry{CreateErrorJSON(req->msg(), resp->error_message()),
                                MySQLEntryStatus::kErr, req_packet.timestamp_ns});
    return true;
  }

  if (IsOKPacket(first_resp_packet)) {
    HandleOKMessage(resp_packets);
  } else {
    // TODO(chengruizhe): Write result set to entry.
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));

    // Nullptr indicates there was not enough packets to process the Resultset.
    // This is not an error, but we return false to indicate that we need more data.
    if (resp == nullptr) {
      return false;
    }
  }

  entries->emplace_back(
      Entry{CreateMessageJSON(req->msg()), MySQLEntryStatus::kOK, req_packet.timestamp_ns});
  return true;
}

StatusOr<bool> StitchFieldList(const Packet& /* req_packet */, std::deque<Packet>* resp_packets,
                               std::vector<Entry>* /* entries */) {
  if (resp_packets->empty()) {
    // Need more data.
    return false;
  }

  return error::Unimplemented("COM_FIELD_LIST is unhandled.");
}

StatusOr<bool> StitchRequestWithBasicResponse(const Packet& /* req_packet */,
                                              std::deque<Packet>* resp_packets,
                                              std::vector<Entry>* entries) {
  if (resp_packets->empty()) {
    // Need more data.
    return false;
  }

  Packet& resp_packet = resp_packets->front();
  ECHECK(IsOKPacket(resp_packet) || IsEOFPacket(resp_packet) || IsErrPacket(resp_packet));
  resp_packets->pop_front();

  // Currently don't record basic request/response events.
  PL_UNUSED(entries);

  return true;
}

StatusOr<Entry> StitchStmtSendLongData(const Packet& req_packet) {
  return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
}

StatusOr<Entry> StitchStmtFetch(const Packet& req_packet, std::deque<Packet>* resp_packets) {
  if (resp_packets->empty()) {
    // Need more data.
    return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
  }

  return error::Unimplemented("COM_STMT_FETCH is unhandled.");
}

StatusOr<Entry> StitchFieldList(const Packet& req_packet, std::deque<Packet>* resp_packets) {
  if (resp_packets->empty()) {
    // Need more data.
    return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
  }

  return error::Unimplemented("COM_FIELD_LIST is unhandled.");
}

StatusOr<Entry> StitchRequestWithBasicResponse(const Packet& req_packet,
                                               std::deque<Packet>* resp_packets) {
  if (resp_packets->empty()) {
    // Need more data.
    return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
  }

  Packet& resp_packet = resp_packets->front();
  ECHECK(IsOKPacket(resp_packet) || IsEOFPacket(resp_packet) || IsErrPacket(resp_packet));
  resp_packets->pop_front();
  return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
