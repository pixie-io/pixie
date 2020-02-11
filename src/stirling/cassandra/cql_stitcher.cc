#include "src/stirling/cassandra/cql_stitcher.h"

#include <deque>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/stirling/cassandra/cass_types.h"
#include "src/stirling/cassandra/type_decoder.h"

namespace pl {
namespace stirling {
namespace cass {

namespace {
std::string BytesToString(std::basic_string_view<uint8_t> x) {
  return pl::BytesToString<PrintStyle::kHexCompact>(CreateStringView<char>(x));
}
}  // namespace

void CheckReqRespPair(const Record& r) {
  PL_UNUSED(r);
  // TODO(oazizi): Add some checks here.
}

Status ProcessSimpleReq(Frame* req_frame, Request* req) {
  req->msg = req_frame->msg;
  return Status::OK();
}

Status ProcessSimpleResp(Frame* resp_frame, Response* resp) {
  resp->msg = std::move(resp_frame->msg);
  return Status::OK();
}

Status ProcessStartupReq(Frame* req_frame, Request* req) {
  TypeDecoder decoder(req_frame->msg);
  PL_ASSIGN_OR_RETURN(StringMap options, decoder.ExtractStringMap());

  DCHECK(req->msg.empty());
  req->msg = utils::ToJSONString(options);

  return Status::OK();
}

Status ProcessAuthResponseReq(Frame* req_frame, Request* req) {
  TypeDecoder decoder(req_frame->msg);
  PL_ASSIGN_OR_RETURN(std::basic_string<uint8_t> token, decoder.ExtractBytes());

  std::string_view token_str = CreateStringView<char>(token);

  DCHECK(req->msg.empty());
  req->msg = token_str;

  return Status::OK();
}

Status ProcessOptionsReq(Frame* req_frame, Request* req) {
  ECHECK(req_frame->hdr.length == 0) << "Options frame not expected to have a body";
  DCHECK(req->msg.empty());
  return Status::OK();
}

Status ProcessRegisterReq(Frame* req_frame, Request* req) {
  TypeDecoder decoder(req_frame->msg);
  PL_ASSIGN_OR_RETURN(StringList event_types, decoder.ExtractStringList());

  DCHECK(req->msg.empty());
  req->msg = utils::ToJSONString(event_types);

  return Status::OK();
}

struct QueryParameters {
  uint16_t consistency;
  uint16_t flags;
  std::vector<std::basic_string<uint8_t>> values;
  std::vector<std::string> names;
  int32_t page_size = 0;
  std::basic_string<uint8_t> paging_state;
  uint16_t serial_consistency = 0;
  int64_t timestamp = 0;
};

StatusOr<QueryParameters> ProcessQueryParameters(TypeDecoder* decoder) {
  QueryParameters qp;

  PL_ASSIGN_OR_RETURN(qp.consistency, decoder->ExtractShort());
  PL_ASSIGN_OR_RETURN(qp.flags, decoder->ExtractByte());

  bool flag_values = qp.flags & 0x01;
  bool flag_skip_metadata = qp.flags & 0x02;
  bool flag_page_size = qp.flags & 0x04;
  bool flag_with_paging_state = qp.flags & 0x08;
  bool flag_with_serial_consistency = qp.flags & 0x10;
  bool flag_with_default_timestamp = qp.flags & 0x20;
  bool flag_with_names_for_values = qp.flags & 0x40;
  PL_UNUSED(flag_skip_metadata);

  if (flag_values) {
    PL_ASSIGN_OR_RETURN(uint16_t num_values, decoder->ExtractShort());
    for (int i = 0; i < num_values; ++i) {
      if (flag_with_names_for_values) {
        PL_ASSIGN_OR_RETURN(std::string name_i, decoder->ExtractString());
        qp.names.push_back(std::move(name_i));
      }
      PL_ASSIGN_OR_RETURN(std::basic_string<uint8_t> value_i, decoder->ExtractBytes());
      qp.values.push_back(std::move(value_i));
    }
  }

  if (flag_page_size) {
    PL_ASSIGN_OR_RETURN(qp.page_size, decoder->ExtractInt());
  }

  if (flag_with_paging_state) {
    PL_ASSIGN_OR_RETURN(qp.paging_state, decoder->ExtractBytes());
  }

  if (flag_with_serial_consistency) {
    PL_ASSIGN_OR_RETURN(qp.serial_consistency, decoder->ExtractShort());
  }

  if (flag_with_default_timestamp) {
    PL_ASSIGN_OR_RETURN(qp.timestamp, decoder->ExtractLong());
  }

  return qp;
}

Status ProcessQueryReq(Frame* req_frame, Request* req) {
  TypeDecoder decoder(req_frame->msg);
  PL_ASSIGN_OR_RETURN(std::string query, decoder.ExtractLongString());
  PL_ASSIGN_OR_RETURN(QueryParameters qp, ProcessQueryParameters(&decoder));

  // TODO(oazizi): This is just a placeholder.
  // Real implementation should figure out what type each value is, and cast into the appropriate
  // type. This, however, will be hard unless we have observed the preceding Prepare request.
  std::vector<std::string> hex_values;
  for (const auto& value_i : qp.values) {
    hex_values.push_back(BytesToString(value_i));
  }

  DCHECK(req->msg.empty());
  req->msg = query;

  // For now, just tag the parameter values to the end.
  // TODO(oazizi): Make this prettier.
  if (!hex_values.empty()) {
    req->msg += "\n";
    req->msg += utils::ToJSONString(hex_values);
  }

  return Status::OK();
}

Status ProcessPrepareReq(Frame* req_frame, Request* req) {
  TypeDecoder decoder(req_frame->msg);
  PL_ASSIGN_OR_RETURN(std::string query, decoder.ExtractLongString());

  DCHECK(req->msg.empty());
  req->msg = query;

  return Status::OK();
}

Status ProcessExecuteReq(Frame* req_frame, Request* req) {
  TypeDecoder decoder(req_frame->msg);
  PL_ASSIGN_OR_RETURN(std::basic_string<uint8_t> id, decoder.ExtractShortBytes());
  PL_ASSIGN_OR_RETURN(QueryParameters qp, ProcessQueryParameters(&decoder));

  // TODO(oazizi): This is just a placeholder.
  // Real implementation should figure out what type each value is, and cast into the appropriate
  // type. This, however, will be hard unless we have observed the preceding Prepare request.
  std::vector<std::string> hex_values;
  for (const auto& value_i : qp.values) {
    hex_values.push_back(BytesToString(value_i));
  }

  DCHECK(req->msg.empty());
  req->msg = utils::ToJSONString(hex_values);

  return Status::OK();
}

Status ProcessErrorResp(Frame* resp_frame, Response* resp) {
  TypeDecoder decoder(resp_frame->msg);
  PL_ASSIGN_OR_RETURN(int32_t error_code, decoder.ExtractInt());
  PL_ASSIGN_OR_RETURN(std::string error_msg, decoder.ExtractString());

  DCHECK(resp->msg.empty());
  resp->msg = absl::Substitute("[$0] $1", error_code, error_msg);

  return Status::OK();
}

Status ProcessReadyResp(Frame* resp_frame, Response* resp) {
  TypeDecoder decoder(resp_frame->msg);
  ECHECK(decoder.eof()) << "READY frame not expected to have a body";

  DCHECK(resp->msg.empty());

  return Status::OK();
}

Status ProcessSupportedResp(Frame* resp_frame, Response* resp) {
  TypeDecoder decoder(resp_frame->msg);
  PL_ASSIGN_OR_RETURN(StringMultiMap options, decoder.ExtractStringMultiMap());

  DCHECK(resp->msg.empty());
  resp->msg = utils::ToJSONString(options);

  return Status::OK();
}

Status ProcessAuthenticateResp(Frame* resp_frame, Response* resp) {
  TypeDecoder decoder(resp_frame->msg);
  PL_ASSIGN_OR_RETURN(std::string authenticator_name, decoder.ExtractString());

  DCHECK(resp->msg.empty());
  resp->msg = std::move(authenticator_name);

  return Status::OK();
}

Status ProcessAuthSuccessResp(Frame* resp_frame, Response* resp) {
  TypeDecoder decoder(resp_frame->msg);
  PL_ASSIGN_OR_RETURN(std::basic_string<uint8_t> token, decoder.ExtractBytes());

  std::string token_hex = BytesToString(token);

  DCHECK(resp->msg.empty());
  resp->msg = token_hex;

  return Status::OK();
}

Status ProcessAuthChallengeResp(Frame* resp_frame, Response* resp) {
  TypeDecoder decoder(resp_frame->msg);
  PL_ASSIGN_OR_RETURN(std::basic_string<uint8_t> token, decoder.ExtractBytes());

  std::string token_hex = BytesToString(token);

  DCHECK(resp->msg.empty());
  resp->msg = token_hex;

  return Status::OK();
}

Status ProcessReq(Frame* req_frame, Request* req) {
  req->op = static_cast<ReqOp>(req_frame->hdr.opcode);
  req->timestamp_ns = req_frame->timestamp_ns;

  switch (req->op) {
    case ReqOp::kStartup:
      return ProcessStartupReq(req_frame, req);
    case ReqOp::kAuthResponse:
      return ProcessAuthResponseReq(req_frame, req);
    case ReqOp::kOptions:
      return ProcessOptionsReq(req_frame, req);
    case ReqOp::kQuery:
      return ProcessQueryReq(req_frame, req);
    case ReqOp::kPrepare:
      return ProcessPrepareReq(req_frame, req);
    case ReqOp::kExecute:
      return ProcessExecuteReq(req_frame, req);
    case ReqOp::kBatch:
      return ProcessSimpleReq(req_frame, req);
    case ReqOp::kRegister:
      return ProcessRegisterReq(req_frame, req);
    default:
      return error::Internal("Unhandled opcode $0", magic_enum::enum_name(req->op));
  }
}

Status ProcessResp(Frame* resp_frame, Response* resp) {
  resp->op = static_cast<RespOp>(resp_frame->hdr.opcode);
  resp->timestamp_ns = resp_frame->timestamp_ns;

  switch (resp->op) {
    case RespOp::kError:
      return ProcessErrorResp(resp_frame, resp);
    case RespOp::kReady:
      return ProcessReadyResp(resp_frame, resp);
    case RespOp::kAuthenticate:
      return ProcessAuthenticateResp(resp_frame, resp);
    case RespOp::kSupported:
      return ProcessSupportedResp(resp_frame, resp);
    case RespOp::kResult:
      return ProcessSimpleResp(resp_frame, resp);
    case RespOp::kEvent:
      return ProcessSimpleResp(resp_frame, resp);
    case RespOp::kAuthChallenge:
      return ProcessAuthChallengeResp(resp_frame, resp);
    case RespOp::kAuthSuccess:
      return ProcessAuthSuccessResp(resp_frame, resp);
    default:
      return error::Internal("Unhandled opcode $0", magic_enum::enum_name(resp->op));
  }
}

StatusOr<Record> ProcessReqRespPair(Frame* req_frame, Frame* resp_frame) {
  Record r;
  PL_RETURN_IF_ERROR(ProcessReq(req_frame, &r.req));
  PL_RETURN_IF_ERROR(ProcessResp(resp_frame, &r.resp));

  ECHECK_LT(req_frame->timestamp_ns, resp_frame->timestamp_ns);
  CheckReqRespPair(r);
  return r;
}

StatusOr<Record> ProcessSolitaryResp(Frame* resp_frame) {
  Record r;

  // For now, Event is the only supported solitary response.
  // If this ever changes, the code below needs to be adapted.
  DCHECK(resp_frame->hdr.opcode == Opcode::kEvent);

  // Make a fake request to go along with the response.
  // - Use REGISTER op, since that was what set up the events in the first place.
  // - Use response timestamp, so any calculated latencies are reported as 0.
  r.req.op = ReqOp::kRegister;
  r.req.msg = "-";
  r.req.timestamp_ns = resp_frame->timestamp_ns;

  // A little inefficient because it will go through a switch statement again,
  // when we actually know the op. But keep it this way for consistency.
  PL_RETURN_IF_ERROR(ProcessResp(resp_frame, &r.resp));

  CheckReqRespPair(r);
  return r;
}

// Currently ProcessFrames() uses a response-led matching algorithm.
// For each response that is at the head of the deque, there should exist a previous request with
// the same stream. Find it, and consume both frames.
// TODO(oazizi): Does it make sense to sort to help the matching?
// Considerations:
//  - Request and response deques are likely (confirm?) to be mostly ordered.
//  - Stream values can be re-used, so sorting would have to consider times too.
//  - Stream values need not be in any sequential order.
std::vector<Record> ProcessFrames(std::deque<Frame>* req_frames, std::deque<Frame>* resp_frames) {
  std::vector<Record> entries;

  for (auto& resp_frame : *resp_frames) {
    bool found_match = false;

    // Event responses are special: they have no request.
    if (resp_frame.hdr.opcode == Opcode::kEvent) {
      StatusOr<Record> record_status = ProcessSolitaryResp(&resp_frame);
      if (record_status.ok()) {
        entries.push_back(record_status.ConsumeValueOrDie());
      } else {
        LOG(ERROR) << record_status.msg();
      }
      resp_frames->pop_front();
      continue;
    }

    // Search for matching req frame
    for (auto& req_frame : *req_frames) {
      if (resp_frame.hdr.stream == req_frame.hdr.stream) {
        VLOG(2) << absl::Substitute("req_op=$0 msg=$1", magic_enum::enum_name(req_frame.hdr.opcode),
                                    req_frame.msg);

        StatusOr<Record> record_status = ProcessReqRespPair(&req_frame, &resp_frame);
        if (record_status.ok()) {
          entries.push_back(record_status.ConsumeValueOrDie());
        } else {
          LOG(ERROR) << record_status.ToString();
        }

        // Found a match, so remove both request and response.
        // We don't remove request frames on the fly, however,
        // because it could otherwise cause unnecessary churn/copying in the deque.
        // This is due to the fact that responses can come out-of-order.
        // Just mark the request as consumed, and clean-up when they reach the head of the queue.
        // Note that responses are always head-processed, so they don't require this optimization.
        found_match = true;
        resp_frames->pop_front();
        req_frame.consumed = true;
        break;
      }
    }

    LOG_IF(ERROR, !found_match) << absl::Substitute(
        "Did not find a request matching the request. Stream = $0", resp_frame.hdr.stream);

    // Clean-up consumed frames at the head.
    // Do this inside the resp loop to aggressively clean-out req_frames whenever a frame consumed.
    // Should speed up the req_frames search for the next iteration.
    for (auto& req_frame : *req_frames) {
      if (!req_frame.consumed) {
        break;
      }
      req_frames->pop_front();
    }

    // TODO(oazizi): Consider removing requests that are too old, otherwise a lost response can mean
    // the are never processed. This would result in a memory leak until the more drastic connection
    // tracker clean-up mechanisms kick in.
  }

  return entries;
}

}  // namespace cass
}  // namespace stirling
}  // namespace pl
