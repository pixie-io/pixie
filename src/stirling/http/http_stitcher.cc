#include "src/stirling/http/http_stitcher.h"

#include <deque>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/common/zlib/zlib_wrapper.h"
#include "src/stirling/http/types.h"
#include "src/stirling/http/utils.h"

// TODO(yzhao): Consider simplify the semantic by filtering entirely on content type.
DEFINE_string(http_response_header_filters, "Content-Type:json",
              "Comma-separated strings to specify the substrings should be included for a header. "
              "The format looks like <header-1>:<substr-1>,...,<header-n>:<substr-n>. "
              "The substrings cannot include comma(s). The filters are conjunctive, "
              "therefore the headers can be duplicate. For example, "
              "'Content-Type:json,Content-Type:text' will select a HTTP response "
              "with a Content-Type header whose value contains 'json' *or* 'text'.");

namespace pl {
namespace stirling {
namespace http {

RecordsWithErrorCount<Record> ProcessMessages(std::deque<Message>* req_messages,
                                              std::deque<Message>* resp_messages) {
  std::vector<http::Record> records;

  // Match request response pairs.
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
      records.push_back(std::move(record));
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
      records.push_back(std::move(record));
      ++resp_iter;
      ++req_iter;
    }
  }

  // Any leftover responses must have lost their requests.
  for (auto resp_iter = resp_messages->begin(); resp_iter != resp_messages->end(); ++resp_iter) {
    Message& resp = *resp_iter;
    Record record{Message(), std::move(resp)};
    records.push_back(std::move(record));
  }
  resp_messages->clear();

  // Any leftover requests are left around for the next iteration,
  // since the response may not have been traced yet.
  // TODO(oazizi): If we have seen the close event, then can assume the response is lost.
  //               We should push the event out in such cases.

  return {records, 0};
}

void PreProcessMessage(Message* message) {
  // Parse the flags on the first time only.
  static const HTTPHeaderFilter kHTTPResponseHeaderFilter =
      ParseHTTPHeaderFilters(FLAGS_http_response_header_filters);

  // Rule: Exclude anything that doesn't specify its Content-Type.
  auto content_type_iter = message->http_headers.find(http::kContentType);
  if (content_type_iter == message->http_headers.end()) {
    message->http_msg_body = "<removed>";
    return;
  }

  // Rule: Exclude anything that doesn't match the filter, if filter is active.
  if (message->type == MessageType::kResponse && (!kHTTPResponseHeaderFilter.inclusions.empty() ||
                                                  !kHTTPResponseHeaderFilter.exclusions.empty())) {
    if (!MatchesHTTPHeaders(message->http_headers, kHTTPResponseHeaderFilter)) {
      message->http_msg_body = "<removed>";
      return;
    }
  }

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
