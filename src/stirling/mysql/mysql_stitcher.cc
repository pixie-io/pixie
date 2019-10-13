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

std::vector<Entry> StitchMySQLPackets(std::deque<Packet>* req_packets,
                                      std::deque<Packet>* resp_packets, mysql::State* state) {
  std::vector<Entry> entries;
  while (!req_packets->empty()) {
    Packet& req_packet = req_packets->front();

    // Command is the first byte.
    char command = req_packet.msg[0];

    StatusOr<Entry> e;
    switch (DecodeEventType(command)) {
      // Internal commands with response: ERR_Packet.
      case MySQLEventType::kConnect:
      case MySQLEventType::kConnectOut:
      case MySQLEventType::kTime:
      case MySQLEventType::kDelayedInsert:
      case MySQLEventType::kDaemon:
        e = StitchRequestWithBasicResponse(req_packet, resp_packets);
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
        e = StitchRequestWithBasicResponse(req_packet, resp_packets);
        break;

      case MySQLEventType::kQuit:  // Response: OK_Packet or a connection close.
        e = StitchRequestWithBasicResponse(req_packet, resp_packets);
        break;

      // Basic Commands with response: EOF_Packet or ERR_Packet.
      case MySQLEventType::kShutdown:  // Deprecated.
      case MySQLEventType::kSetOption:
      case MySQLEventType::kDebug:
        e = StitchRequestWithBasicResponse(req_packet, resp_packets);
        break;

      // COM_FIELD_LIST has its own COM_FIELD_LIST meta response (ERR_Packet or one or more Column
      // Definition packets and a closing EOF_Packet).
      case MySQLEventType::kFieldList:  // Deprecated.
        e = StitchFieldList(req_packet, resp_packets);
        break;

      // COM_QUERY has its own COM_QUERY meta response (ERR_Packet, OK_Packet,
      // Protocol::LOCAL_INFILE_Request, or ProtocolText::Resultset).
      case MySQLEventType::kQuery:
        e = StitchQuery(req_packet, resp_packets);
        break;

      // COM_STMT_PREPARE returns COM_STMT_PREPARE_OK on success, ERR_Packet otherwise.
      case MySQLEventType::kStmtPrepare:
        e = StitchStmtPrepare(req_packet, resp_packets, state);
        break;

      // COM_STMT_SEND_LONG_DATA has no response.
      case MySQLEventType::kStmtSendLongData:
        e = StitchStmtSendLongData(req_packet);
        break;

      // COM_STMT_EXECUTE has its own COM_STMT_EXECUTE meta response (OK_Packet, ERR_Packet or a
      // resultset: Binary Protocol Resultset).
      case MySQLEventType::kStmtExecute:
        e = StitchStmtExecute(req_packet, resp_packets, state);
        break;

      // COM_CLOSE has no response.
      case MySQLEventType::kStmtClose:
        e = StitchStmtClose(req_packet, state);
        break;

      // COM_STMT_RESET response is OK_Packet if the statement could be reset, ERR_Packet if not.
      case MySQLEventType::kStmtReset:
        e = StitchRequestWithBasicResponse(req_packet, resp_packets);
        break;

      // COM_STMT_FETCH has a meta response (multi-resultset, or ERR_Packet).
      case MySQLEventType::kStmtFetch:
        e = StitchStmtFetch(req_packet, resp_packets);
        break;

      case MySQLEventType::kProcessInfo:     // a ProtocolText::Resultset or ERR_Packet
      case MySQLEventType::kChangeUser:      // Authentication Method Switch Request Packet or
                                             // ERR_Packet
      case MySQLEventType::kBinlogDumpGTID:  // binlog network stream, ERR_Packet or EOF_Packet
      case MySQLEventType::kBinlogDump:      // binlog network stream, ERR_Packet or EOF_Packet
      case MySQLEventType::kTableDump:       // a table dump or ERR_Packet
      case MySQLEventType::kStatistics:      // string.EOF
        // Rely on recovery to re-sync responses based on timestamps.
        e = error::Internal("Unimplemented command $0.", command);
        break;

      default:
        e = error::Internal("Unknown command $0.", command);
        break;
    }

    if (e.ok()) {
      Entry entry = e.ValueOrDie();
      // StitcherStmtPrepare returns a StatusOr<Entry> with Unknown Status, since it's not ready to
      // be emitted yet.
      if (entry.status != MySQLEntryStatus::kUnknown) {
        entries.push_back(entry);
      }
      req_packets->pop_front();
    } else {
      LOG(WARNING) << e.msg();
      break;
    }
  }
  return entries;
}

StatusOr<Entry> StitchStmtPrepare(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                  mysql::State* state) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  Packet& header_packet = resp_packets->front();
  if (IsErrPacket(header_packet)) {
    auto resp = HandleErrMessage(resp_packets);
    return Entry{CreateErrorJSON(req->msg(), resp->error_message()), MySQLEntryStatus::kErr,
                 req_packet.timestamp_ns};

  } else {
    PL_ASSIGN_OR_RETURN(auto resp, HandleStmtPrepareOKResponse(resp_packets));
    int stmt_id = resp->resp_header().stmt_id;
    state->prepare_events.emplace(
        stmt_id, ReqRespEvent(MySQLEventType::kStmtPrepare, std::move(req), std::move(resp)));
    return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
  }
}

StatusOr<Entry> StitchStmtExecute(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                  mysql::State* state) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStmtExecuteRequest(req_packet, &state->prepare_events));

  Packet& first_resp_packet = resp_packets->front();

  // Assuming that if corresponding StmtPrepare is not found, and the first response packet is
  // an error, client made a mistake, so we pop off the error response.
  if (req->stmt_id() == -1) {
    if (IsErrPacket(first_resp_packet)) {
      auto resp = HandleErrMessage(resp_packets);
      std::string error_msg = absl::Substitute(R"({"Error": "$0"})", resp->error_message());
      return Entry{error_msg, MySQLEntryStatus::kErr, req_packet.timestamp_ns};
    } else {
      // TODO(chengruizhe): If the response packet is a resultset, it's likely that we missed
      // the StmtPrepare. Identify this case, and pop off the resultset to avoid confusion.
      return error::Cancelled("StitchStmtExecute: StmtExecute received on deleted StmtPrepare.");
    }
  }

  std::string error_message;
  if (IsOKPacket(first_resp_packet)) {
    HandleOKMessage(resp_packets);
  } else if (IsErrPacket(first_resp_packet)) {
    auto resp = HandleErrMessage(resp_packets);

    error_message = resp->error_message();
  } else {
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));
  }

  // TODO(chengruizhe): Write result set to entry.
  std::string filled_msg = CombinePrepareExecute(req.get(), &state->prepare_events);
  if (error_message.empty()) {
    return Entry{CreateMessageJSON(filled_msg), MySQLEntryStatus::kOK, req_packet.timestamp_ns};
  } else {
    return Entry{CreateErrorJSON(filled_msg, error_message), MySQLEntryStatus::kErr,
                 req_packet.timestamp_ns};
  }
}

StatusOr<Entry> StitchStmtClose(const Packet& req_packet, State* state) {
  PL_RETURN_IF_ERROR(HandleStmtCloseRequest(req_packet, &state->prepare_events));
  return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
}

StatusOr<Entry> StitchQuery(const Packet& req_packet, std::deque<Packet>* resp_packets) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  Packet& first_resp_packet = resp_packets->front();
  if (IsOKPacket(first_resp_packet)) {
    auto resp = HandleOKMessage(resp_packets);
  } else if (IsErrPacket(first_resp_packet)) {
    auto resp = HandleErrMessage(resp_packets);
    return Entry{CreateErrorJSON(req->msg(), resp->error_message()), MySQLEntryStatus::kErr,
                 req_packet.timestamp_ns};
  } else {
    // TODO(chengruizhe): Write result set to entry.
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));
  }

  return Entry{CreateMessageJSON(req->msg()), MySQLEntryStatus::kOK, req_packet.timestamp_ns};
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
