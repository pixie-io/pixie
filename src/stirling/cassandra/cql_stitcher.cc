#include "src/stirling/cassandra/cql_stitcher.h"

#include <deque>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/stirling/cassandra/cass_types.h"
#include "src/stirling/cassandra/cql_types.h"

namespace pl {
namespace stirling {
namespace cass {

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
  std::string_view body = req_frame->msg;
  PL_ASSIGN_OR_RETURN(StringMap options, ExtractStringMap(&body));

  DCHECK(req->msg.empty());
  req->msg = utils::ToJSONString(options);

  return Status::OK();
}

Status ProcessRegisterReq(Frame* req_frame, Request* req) {
  std::string_view body = req_frame->msg;
  PL_ASSIGN_OR_RETURN(StringList event_types, ExtractStringList(&body));

  DCHECK(req->msg.empty());
  req->msg = utils::ToJSONString(event_types);

  return Status::OK();
}

Status ProcessSupportedResp(Frame* resp_frame, Response* resp) {
  std::string_view body = resp_frame->msg;
  PL_ASSIGN_OR_RETURN(StringMultiMap options, ExtractStringMultiMap(&body));

  DCHECK(resp->msg.empty());
  resp->msg = utils::ToJSONString(options);

  return Status::OK();
}

Status ProcessReq(Frame* req_frame, Request* req) {
  req->op = static_cast<ReqOp>(req_frame->hdr.opcode);
  req->timestamp_ns = req_frame->timestamp_ns;

  switch (req->op) {
    case ReqOp::kStartup:
      return ProcessStartupReq(req_frame, req);
    case ReqOp::kAuthResponse:
      return ProcessSimpleReq(req_frame, req);
    case ReqOp::kOptions:
      return ProcessSimpleReq(req_frame, req);
    case ReqOp::kQuery:
      return ProcessSimpleReq(req_frame, req);
    case ReqOp::kPrepare:
      return ProcessSimpleReq(req_frame, req);
    case ReqOp::kExecute:
      return ProcessSimpleReq(req_frame, req);
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
      return ProcessSimpleResp(resp_frame, resp);
    case RespOp::kReady:
      return ProcessSimpleResp(resp_frame, resp);
    case RespOp::kAuthenticate:
      return ProcessSimpleResp(resp_frame, resp);
    case RespOp::kSupported:
      return ProcessSupportedResp(resp_frame, resp);
    case RespOp::kResult:
      return ProcessSimpleResp(resp_frame, resp);
    case RespOp::kEvent:
      return ProcessSimpleResp(resp_frame, resp);
    case RespOp::kAuthChallenge:
      return ProcessSimpleResp(resp_frame, resp);
    case RespOp::kAuthSuccess:
      return ProcessSimpleResp(resp_frame, resp);
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
