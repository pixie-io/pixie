#include "src/stirling/protocols/dns/dns_stitcher.h"

#include <deque>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/protocols/dns/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace dns {

void ProcessReq(const Frame& req_frame, Request* req) {
  req->timestamp_ns = req_frame.timestamp_ns;
}

void ProcessResp(const Frame& resp_frame, Response* resp) {
  resp->timestamp_ns = resp_frame.timestamp_ns;

  resp->msg.clear();
  absl::StrAppend(&resp->msg, "Queries: [\n");
  for (const auto& r : resp_frame.records) {
    absl::StrAppend(&resp->msg, r.name, r.addr.AddrStr());
  }
  absl::StrAppend(&resp->msg, "]\n");
}

StatusOr<Record> ProcessReqRespPair(const Frame& req_frame, const Frame& resp_frame) {
  ECHECK_LT(req_frame.timestamp_ns, resp_frame.timestamp_ns);

  Record r;
  ProcessReq(req_frame, &r.req);
  ProcessResp(resp_frame, &r.resp);

  return r;
}

// Currently ProcessFrames() uses a response-led matching algorithm.
// For each response that is at the head of the deque, there should exist a previous request with
// the same txid. Find it, and consume both frames.
RecordsWithErrorCount<Record> ProcessFrames(std::deque<Frame>* req_frames,
                                            std::deque<Frame>* resp_frames) {
  std::vector<Record> entries;
  int error_count = 0;

  for (auto& resp_frame : *resp_frames) {
    bool found_match = false;

    // Search for matching req frame
    for (auto& req_frame : *req_frames) {
      if (resp_frame.header.txid == req_frame.header.txid) {
        StatusOr<Record> record_status = ProcessReqRespPair(req_frame, resp_frame);
        if (record_status.ok()) {
          entries.push_back(record_status.ConsumeValueOrDie());
        } else {
          VLOG(1) << record_status.ToString();
          ++error_count;
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

    if (!found_match) {
      VLOG(1) << absl::Substitute("Did not find a request matching the response. TXID = $0",
                                  resp_frame.header.txid);
      ++error_count;
    }

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

  return {entries, error_count};
}

}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
