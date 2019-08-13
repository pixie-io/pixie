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
    return "";
  }

  std::string_view stmt_prepare_request =
      static_cast<StringRequest*>(iter->second.request())->msg();

  size_t offset = 0;
  size_t count = 0;
  std::string result = "";

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
std::string CreateErrorJSON(std::string_view body, std::string_view error) {
  if (error == "") {
    return absl::StrCat("{\"Message\": \"", body, "\"}");
  }
  return absl::StrCat("{\"Error\": \"", error, "\", \"Message\": \"", body, "\"}");
}

}  // namespace

std::vector<Entry> StitchMySQLPackets(std::deque<Packet>* req_packets,
                                      std::deque<Packet>* resp_packets,
                                      std::map<int, ReqRespEvent>* prepare_events) {
  std::vector<Entry> entries;
  while (!req_packets->empty()) {
    if (resp_packets->empty()) {
      break;
    }

    Packet& req_packet = req_packets->front();

    StatusOr<Entry> e;
    // TODO(chengruizhe): Remove type from Packet and infer type here.
    switch (req_packet.type) {
      case MySQLEventType::kComStmtPrepare:
        e = StitchStmtPrepare(req_packet, resp_packets, prepare_events);
        break;
      case MySQLEventType::kComStmtExecute:
        e = StitchStmtExecute(req_packet, resp_packets, prepare_events);
        break;
      case MySQLEventType::kComStmtClose:
        e = StitchStmtClose(req_packet, prepare_events);
        break;
      case MySQLEventType::kComQuery:
        e = StitchQuery(req_packet, resp_packets);
        break;
      case MySQLEventType::kUnknown:
        // TODO(chengruizhe): Here we assume that if the request type is unknown, the response will
        // be just one packet. Make it more robust.
        resp_packets->pop_front();
        LOG(WARNING) << "Unknown MySQL event type in stitcher";
        continue;
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
                                  std::map<int, ReqRespEvent>* prepare_events) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  Packet header_packet = resp_packets->front();
  if (IsErrPacket(header_packet)) {
    PL_ASSIGN_OR_RETURN(auto resp, HandleErrMessage(resp_packets));
    return Entry{CreateErrorJSON(req->msg(), resp->error_message()), MySQLEntryStatus::kErr,
                 req_packet.timestamp_ns};

  } else {
    PL_ASSIGN_OR_RETURN(auto resp, HandleStmtPrepareOKResponse(resp_packets));
    int stmt_id = resp->resp_header().stmt_id;
    prepare_events->emplace(
        stmt_id, ReqRespEvent(MySQLEventType::kComStmtPrepare, std::move(req), std::move(resp)));
    return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
  }
}

StatusOr<Entry> StitchStmtExecute(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                  std::map<int, ReqRespEvent>* prepare_events) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStmtExecuteRequest(req_packet, prepare_events));

  Packet first_packet = resp_packets->front();
  std::string error_message = "";
  if (IsOKPacket(first_packet)) {
    PL_ASSIGN_OR_RETURN(auto resp, HandleOKMessage(resp_packets));

  } else if (IsErrPacket(first_packet)) {
    PL_ASSIGN_OR_RETURN(auto resp, HandleErrMessage(resp_packets));

    error_message = resp->error_message();

  } else {
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));
  }

  // TODO(chengruizhe): Write result set to entry.
  std::string filled_msg = CombinePrepareExecute(req.get(), prepare_events);
  if (error_message == "") {
    return Entry{CreateErrorJSON(filled_msg, error_message), MySQLEntryStatus::kOK,
                 req_packet.timestamp_ns};
  } else {
    return Entry{CreateErrorJSON(filled_msg, error_message), MySQLEntryStatus::kErr,
                 req_packet.timestamp_ns};
  }
}

StatusOr<Entry> StitchStmtClose(const Packet& req_packet,
                                std::map<int, ReqRespEvent>* prepare_events) {
  PL_RETURN_IF_ERROR(HandleStmtCloseRequest(req_packet, prepare_events));
  return Entry{"", MySQLEntryStatus::kUnknown, req_packet.timestamp_ns};
}

StatusOr<Entry> StitchQuery(const Packet& req_packet, std::deque<Packet>* resp_packets) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  Packet first_packet = resp_packets->front();
  if (IsOKPacket(first_packet)) {
    PL_ASSIGN_OR_RETURN(auto resp, HandleOKMessage(resp_packets));

  } else if (IsErrPacket(first_packet)) {
    PL_ASSIGN_OR_RETURN(auto resp, HandleErrMessage(resp_packets));
    return Entry{CreateErrorJSON(req->msg(), resp->error_message()), MySQLEntryStatus::kErr,
                 req_packet.timestamp_ns};

  } else {
    // TODO(chengruizhe): Write result set to entry.
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));
  }

  return Entry{CreateErrorJSON(req->msg(), ""), MySQLEntryStatus::kOK, req_packet.timestamp_ns};
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
