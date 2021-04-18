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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http {

TEST(HTTPHeadersMap, CaseInsensitivity) {
  const HeadersMap kHTTPHeaders = {
      {"Content-Type", "application/json"},
      {"Transfer-Encoding", "chunked"},
  };

  // Check that we can access the key regardless of the case of the key (content-type).
  {
    auto http_headers_iter = kHTTPHeaders.find("content-type");
    ASSERT_NE(http_headers_iter, kHTTPHeaders.end());
    EXPECT_EQ(http_headers_iter->second, "application/json");
  }

  {
    auto http_headers_iter = kHTTPHeaders.find("Content-Type");
    ASSERT_NE(http_headers_iter, kHTTPHeaders.end());
    EXPECT_EQ(http_headers_iter->second, "application/json");
  }
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
