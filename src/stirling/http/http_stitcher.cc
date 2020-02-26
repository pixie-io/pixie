#include "src/stirling/http/http_stitcher.h"

#include <deque>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/common/zlib/zlib_wrapper.h"
#include "src/stirling/http/types.h"

namespace pl {
namespace stirling {
namespace http {

std::vector<Record> ProcessMessages(std::deque<Message>* req_messages,
                                    std::deque<Message>* resp_messages) {
  // Match request response pairs.
  std::vector<http::Record> trace_records;
  for (auto req_iter = req_messages->begin(), resp_iter = resp_messages->begin();
       req_iter != req_messages->end() && resp_iter != resp_messages->end();) {
    Message& req = *req_iter;
    Message& resp = *resp_iter;

    if (resp.timestamp_ns < req.timestamp_ns) {
      // Oldest message is a response.
      // This means the corresponding request was not traced.
      // Push without request.
      Record record{Message(), std::move(resp)};
      resp_messages->pop_front();
      trace_records.push_back(std::move(record));
      ++resp_iter;
    } else {
      // Found a response. It must be the match assuming:
      //  1) In-order messages (which is true for HTTP1).
      //  2) No missing messages.
      // With missing messages, there are no guarantees.
      // With no missing messages and pipelining, it's even more complicated.
      Record record{std::move(req), std::move(resp)};
      req_messages->pop_front();
      resp_messages->pop_front();
      trace_records.push_back(std::move(record));
      ++resp_iter;
      ++req_iter;
    }
  }

  // Any leftover responses must have lost their requests.
  for (auto resp_iter = resp_messages->begin(); resp_iter != resp_messages->end(); ++resp_iter) {
    Message& resp = *resp_iter;
    Record record{Message(), std::move(resp)};
    trace_records.push_back(std::move(record));
  }
  resp_messages->clear();

  // Any leftover requests are left around for the next iteration,
  // since the response may not have been traced yet.
  // TODO(oazizi): If we have seen the close event, then can assume the response is lost.
  //               We should push the event out in such cases.

  return trace_records;
}

void PreProcessMessage(Message* message) {
  auto content_encoding_iter = message->http_headers.find(kContentEncoding);
  // Replace body with decompressed version, if required.
  if (content_encoding_iter != message->http_headers.end() &&
      content_encoding_iter->second == "gzip") {
    std::string_view body_strview(message->http_msg_body);
    auto bodyOrErr = pl::zlib::Inflate(body_strview);
    if (!bodyOrErr.ok()) {
      LOG(WARNING) << "Unable to gunzip HTTP body.";
      message->http_msg_body = "<Stirling failed to gunzip body>";
    } else {
      message->http_msg_body = bodyOrErr.ValueOrDie();
    }
  }
}

}  // namespace http
}  // namespace stirling
}  // namespace pl
