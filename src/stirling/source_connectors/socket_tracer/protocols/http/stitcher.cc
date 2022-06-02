/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/socket_tracer/protocols/http/stitcher.h"

#include <deque>
#include <limits>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/common/zlib/zlib_wrapper.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/utils.h"

// TODO(yzhao): Consider simplify the semantic by filtering entirely on content type.
DEFINE_string(http_response_header_filters, "Content-Type:json,Content-Type:text/",
              "Comma-separated strings to specify the substrings should be included for a header. "
              "The format looks like <header-1>:<substr-1>,...,<header-n>:<substr-n>. "
              "The substrings cannot include comma(s). The filters are conjunctive, "
              "therefore the headers can be duplicate. For example, "
              "'Content-Type:json,Content-Type:text' will select a HTTP response "
              "with a Content-Type header whose value contains 'json' *or* 'text'.");

namespace px {
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
    if (message->body_size > 0) {
      // Don't rewrite if the body is empty.
      message->body = "<removed: unknown content-type>";
    }
    return;
  }

  // Rule: Exclude anything that doesn't match the filter, if filter is active.
  if (message->type == message_type_t::kResponse &&
      (!kHTTPResponseHeaderFilter.inclusions.empty() ||
       !kHTTPResponseHeaderFilter.exclusions.empty())) {
    if (!MatchesHTTPHeaders(message->headers, kHTTPResponseHeaderFilter)) {
      message->body = "<removed: non-text content-type>";
      return;
    }
  }

  auto content_encoding_iter = message->headers.find(kContentEncoding);
  // Replace body with decompressed version, if required.
  if (content_encoding_iter != message->headers.end() && content_encoding_iter->second == "gzip") {
    message->body = px::zlib::Inflate(message->body).ConsumeValueOr("<Failed to gunzip body>");
  }
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
