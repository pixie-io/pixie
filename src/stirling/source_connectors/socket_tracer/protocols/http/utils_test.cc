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

#include "src/stirling/source_connectors/socket_tracer/protocols/http/utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(ParseHTTPHeaderFiltersAndMatchTest, FiltersAreAsExpectedAndMatchesWork) {
  const std::string filters_string =
      "Content-Type:json,,,,Content-Type:plain,Transfer-Encoding:chunked,,,Transfer-Encoding:"
      "chunked,-Content-Encoding:gzip,-Content-Encoding:binary";
  HTTPHeaderFilter filter = ParseHTTPHeaderFilters(filters_string);
  EXPECT_THAT(filter.inclusions,
              ElementsAre(Pair("Content-Type", "json"), Pair("Content-Type", "plain"),
                          Pair("Transfer-Encoding", "chunked"),
                          // Note that multimap does not remove duplicates.
                          Pair("Transfer-Encoding", "chunked")));
  EXPECT_THAT(filter.exclusions,
              ElementsAre(Pair("Content-Encoding", "gzip"), Pair("Content-Encoding", "binary")));
  {
    HeadersMap http_headers = {
        {"Content-Type", "application/json; charset=utf-8"},
    };
    EXPECT_TRUE(MatchesHTTPHeaders(http_headers, filter));
    http_headers.insert({"Content-Encoding", "gzip"});
    EXPECT_FALSE(MatchesHTTPHeaders(http_headers, filter)) << "gzip should be filtered out";
  }
  {
    HeadersMap http_headers = {
        {"Transfer-Encoding", "chunked"},
    };
    EXPECT_TRUE(MatchesHTTPHeaders(http_headers, filter));
    http_headers.insert({"Content-Encoding", "binary"});
    EXPECT_FALSE(MatchesHTTPHeaders(http_headers, filter)) << "binary should be filtered out";
  }
  {
    HeadersMap http_headers;
    EXPECT_FALSE(MatchesHTTPHeaders(http_headers, filter));

    const HTTPHeaderFilter empty_filter;
    EXPECT_TRUE(MatchesHTTPHeaders(http_headers, empty_filter))
        << "Empty filter matches any HTTP headers";
    http_headers.insert({"Content-Type", "non-matching-type"});
    EXPECT_TRUE(MatchesHTTPHeaders(http_headers, empty_filter))
        << "Empty filter matches any HTTP headers";
  }
  {
    const HeadersMap http_headers = {
        {"Content-Type", "non-matching-type"},
    };
    EXPECT_FALSE(MatchesHTTPHeaders(http_headers, filter));
  }
}

TEST(HTTPUrlDecode, Decode) {
  std::string input =
      "action=%7B%0A++%22commands%22%3A+%5B%0A++++%7B%0A++++++%22server%22%3A+%22api.use-case.svc."
      "cluster.local%3A5011%22%2C%0A++++++%22action%22%3A+%22req2%22%2C%0A++++++%22telemetry";
  std::string expected = R"(action={
  "commands": [
    {
      "server": "api.use-case.svc.cluster.local:5011",
      "action": "req2",
      "telemetry)";
  EXPECT_EQ(HTTPUrlDecode(input), expected);
}

TEST(HTTPUrlDecode, DecodeTruncatedInput) {
  // Use input that has its first % encoded hex digit removed
  std::string truncated_first_digit = "action=%";
  EXPECT_EQ(HTTPUrlDecode(truncated_first_digit), truncated_first_digit);

  // Use input that has its second % encoded hex digit removed
  std::string truncated_second_digit = "action=%7";
  EXPECT_EQ(HTTPUrlDecode(truncated_second_digit), truncated_second_digit);
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
