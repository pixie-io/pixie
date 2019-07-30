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
// TODO(chengruizhe): Implement in next diff.
std::string CombinePrepareExecute(const StmtExecuteRequest* req,
                                  std::map<int, ReqRespEvent>* prepare_events) {
  PL_UNUSED(req);
  PL_UNUSED(prepare_events);
  return "pixie";
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
  for (Packet& req_packet : *req_packets) {
    if (resp_packets->empty()) {
      break;
    }

    StatusOr<Entry> e;
    // TODO(chengruizhe): Remove type from Packet and infer type here.
    switch (req_packet.type) {
      case MySQLEventType::kComStmtPrepare:
        e = StitchStmtPrepare(req_packet, resp_packets, prepare_events);
        break;
      case MySQLEventType::kComStmtExecute:
        e = StitchStmtExecute(req_packet, resp_packets, prepare_events);
        break;
      case MySQLEventType::kComQuery:
        e = StitchQuery(req_packet, resp_packets);
        break;
      case MySQLEventType::kUnknown:
        // TODO(chengruizhe): Here we assume that if the request type is unknown, the response will
        // be just one packet. Make it more robost.
        resp_packets->pop_back();
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
    } else {
      LOG(WARNING) << e.msg();
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
    return Entry{CreateErrorJSON(req->msg(), resp->error_message()), MySQLEntryStatus::kErr};

  } else {
    PL_ASSIGN_OR_RETURN(auto resp, HandleStmtPrepareOKResponse(resp_packets, prepare_events));

    ReqRespEvent event(MySQLEventType::kComStmtPrepare, std::move(req), std::move(resp));
    return Entry{"", MySQLEntryStatus::kUnknown};
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
    return Entry{CreateErrorJSON(filled_msg, error_message), MySQLEntryStatus::kOK};
  } else {
    return Entry{CreateErrorJSON(filled_msg, error_message), MySQLEntryStatus::kErr};
  }
}

StatusOr<Entry> StitchQuery(const Packet& req_packet, std::deque<Packet>* resp_packets) {
  PL_ASSIGN_OR_RETURN(auto req, HandleStringRequest(req_packet));

  Packet first_packet = resp_packets->front();
  if (IsOKPacket(first_packet)) {
    PL_ASSIGN_OR_RETURN(auto resp, HandleOKMessage(resp_packets));

  } else if (IsErrPacket(first_packet)) {
    PL_ASSIGN_OR_RETURN(auto resp, HandleErrMessage(resp_packets));
    return Entry{CreateErrorJSON(req->msg(), resp->error_message()), MySQLEntryStatus::kErr};

  } else {
    // TODO(chengruizhe): Write result set to entry.
    PL_ASSIGN_OR_RETURN(auto resp, HandleResultset(resp_packets));
  }

  return Entry{CreateErrorJSON(req->msg(), ""), MySQLEntryStatus::kOK};
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
