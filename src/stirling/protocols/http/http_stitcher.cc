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
