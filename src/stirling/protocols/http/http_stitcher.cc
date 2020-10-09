#include "src/stirling/protocols/http/http_stitcher.h"

#include <deque>
#include <limits>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/common/zlib/zlib_wrapper.h"
#include "src/stirling/protocols/http/types.h"
#include "src/stirling/protocols/http/utils.h"

// TODO(yzhao): Consider simplify the semantic by filtering entirely on content type.
DEFINE_string(http_response_header_filters, "Content-Type:json,Content-Type:text/",
              "Comma-separated strings to specify the substrings should be included for a header. "
              "The format looks like <header-1>:<substr-1>,...,<header-n>:<substr-n>. "
              "The substrings cannot include comma(s). The filters are conjunctive, "
              "therefore the headers can be duplicate. For example, "
              "'Content-Type:json,Content-Type:text' will select a HTTP response "
              "with a Content-Type header whose value contains 'json' *or* 'text'.");

namespace pl {
namespace stirling {
namespace protocols {
namespace http {

RecordsWithErrorCount<Record> ProcessMessages(std::deque<Message>* req_messages,
                                              std::deque<Message>* resp_messages) {
  // This function performs request response matching.
  // NOTICE: Assume no HTTP pipelining to match requests and responses.
  // Implementation consists of a merge-sort of the two deques.

  std::vector<http::Record> records;

  Record record;
  record.req.timestamp_ns = 0;
  record.resp.timestamp_ns = 0;

  Message dummy_message;
  dummy_message.timestamp_ns = std::numeric_limits<int64_t>::max();

  // Each iteration, we pop off either a request or response message.
  auto req_iter = req_messages->begin();
  auto resp_iter = resp_messages->begin();
  while (resp_iter != resp_messages->end()) {
    Message& req = (req_iter == req_messages->end()) ? dummy_message : *req_iter;
    Message& resp = (resp_iter == resp_messages->end()) ? dummy_message : *resp_iter;

    // Process the oldest item, either a request or response.
    if (req.timestamp_ns < resp.timestamp_ns) {
      // Requests always go into the record (though not pushed yet).
      // If the next oldest item is a request too, it will (correctly) clobber this one.
      record.req = std::move(req);
      ++req_iter;
    } else {
      // Two cases for a response:
      // 1) No older request was found: then we ignore the response.
      // 2) An older request was found: then it is considered a match. Push the record, and reset.
      if (record.req.timestamp_ns != 0) {
        record.resp = std::move(resp);

        records.push_back(std::move(record));

        // Reset record after pushing.
        record.req.timestamp_ns = 0;
        record.resp.timestamp_ns = 0;
      }
      ++resp_iter;
    }
  }

  req_messages->erase(req_messages->begin(), req_iter);
  resp_messages->erase(resp_messages->begin(), resp_iter);

  return {records, 0};
}

void PreProcessMessage(Message* message) {
  // Parse the flags on the first time only.
  static const HTTPHeaderFilter kHTTPResponseHeaderFilter =
      ParseHTTPHeaderFilters(FLAGS_http_response_header_filters);

  // Rule: Exclude anything that doesn't specify its Content-Type.
  auto content_type_iter = message->headers.find(http::kContentType);
  if (content_type_iter == message->headers.end()) {
    message->body = "<removed: unknown content-type>";
    return;
  }

  // Rule: Exclude anything that doesn't match the filter, if filter is active.
  if (message->type == MessageType::kResponse && (!kHTTPResponseHeaderFilter.inclusions.empty() ||
                                                  !kHTTPResponseHeaderFilter.exclusions.empty())) {
    if (!MatchesHTTPHeaders(message->headers, kHTTPResponseHeaderFilter)) {
      message->body = "<removed: non-text content-type>";
      return;
    }
  }

  auto content_encoding_iter = message->headers.find(kContentEncoding);
  // Replace body with decompressed version, if required.
  if (content_encoding_iter != message->headers.end() && content_encoding_iter->second == "gzip") {
    std::string_view body_strview(message->body);
    auto bodyOrErr = pl::zlib::Inflate(body_strview);
    if (!bodyOrErr.ok()) {
      LOG(WARNING) << "Unable to gunzip HTTP body.";
      message->body = "<Failed to gunzip body>";
    } else {
      message->body = bodyOrErr.ValueOrDie();
    }
  }
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
