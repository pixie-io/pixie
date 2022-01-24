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

#include "src/stirling/source_connectors/socket_tracer/protocols/http/body_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http {

size_t kBodySizeLimitBytes = 1000000;

struct TestParams {
  bool use_pico_chunked_decoder;
};

class ChunkedDecoderTest : public ::testing::TestWithParam<TestParams> {
 public:
  ChunkedDecoderTest() { FLAGS_use_pico_chunked_decoder = false; }
  void SetUp() {
    auto params = GetParam();
    FLAGS_use_pico_chunked_decoder = params.use_pico_chunked_decoder;
  }
};

INSTANTIATE_TEST_SUITE_P(ChunkedDecoderTestSuite, ChunkedDecoderTest,
                         ::testing::Values(TestParams{true}, TestParams{false}));

TEST_P(ChunkedDecoderTest, Basic) {
  std::string_view body =
      "9\r\n"
      "pixielabs\r\n"
      "14\r\n"
      " is so very awesome!\r\n"
      "0\r\n"
      "\r\n";

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  EXPECT_EQ(result, ParseState::kSuccess);
  EXPECT_EQ(out, "pixielabs is so very awesome!");
  EXPECT_TRUE(body.empty());
}

TEST_P(ChunkedDecoderTest, IncludingNext) {
  std::string_view body =
      "9\r\n"
      "pixielabs\r\n"
      "C\r\n"
      " is awesome!\r\n"
      "0\r\n"
      "\r\n"
      "HTTP/1.1 200 OK\r\n";

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  EXPECT_EQ(result, ParseState::kSuccess);
  EXPECT_EQ(out, "pixielabs is awesome!");
  EXPECT_EQ(body, std::string(CreateStringView<char>("HTTP/1.1 200 OK\r\n")));
}

TEST_P(ChunkedDecoderTest, Empty) {
  std::string_view body =
      "0\r\n"
      "\r\n";

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  EXPECT_EQ(result, ParseState::kSuccess);
  EXPECT_EQ(out, "");
  EXPECT_EQ(body, "");
}

TEST_P(ChunkedDecoderTest, Incomplete) {
  std::string_view body =
      "9\r\n"
      "pixielabs\r\n"
      "C\r\n"
      " is awesome!\r\n"
      "0\r\n"
      "\r\n";
  std::string original(body);

  for (size_t i = 0; i < body.size(); ++i) {
    std::string_view body_substr = body.substr(0, i);

    std::string out;
    size_t body_size;
    ParseState result = ParseChunked(&body_substr, kBodySizeLimitBytes, &out, &body_size);

    EXPECT_EQ(result, ParseState::kNeedsMoreData);
    EXPECT_EQ(out, "");
    EXPECT_EQ(body, original);
  }
}

TEST_P(ChunkedDecoderTest, InconsistentLength) {
  std::string_view body =
      "B\r\n"
      "pixielabs\r\n"
      "E\r\n"
      " is awesome!\r\n"
      "0\r\n"
      "\r\n";
  std::string original(body);

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  EXPECT_EQ(result, ParseState::kInvalid);
  EXPECT_EQ(out, "");
  EXPECT_EQ(body, original);
}

TEST_P(ChunkedDecoderTest, UnexpectedTerminatorInLength) {
  std::string_view body =
      "9\r\n"
      "pixielabs\r\n"
      "C\rx"
      " is awesome!\r\n"
      "0\r\n"
      "\r\n";
  std::string original(body);

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  auto params = GetParam();

  // The two implementations differ in behavior.
  // Our custom decoder applies the spec more strictly.
  EXPECT_EQ(result,
            params.use_pico_chunked_decoder ? ParseState::kNeedsMoreData : ParseState::kInvalid);
  EXPECT_EQ(out, "");
  EXPECT_EQ(body, original);
}

TEST_P(ChunkedDecoderTest, UnexpectedTerminatorInData) {
  std::string_view body =
      "9\r\n"
      "pixielabs\r\n"
      "C\r\n"
      " is awesome!\rx"
      "0\r\n"
      "\r\n";
  std::string original(body);

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  EXPECT_EQ(result, ParseState::kInvalid);
  EXPECT_EQ(out, "");
  EXPECT_EQ(body, original);
}

TEST_P(ChunkedDecoderTest, UnexpectedTerminatorAtEnd) {
  std::string_view body =
      "9\r\n"
      "pixielabs\r\n"
      "C\r\n"
      " is awesome!\rx"
      "0\r\n"
      "\rx";
  std::string original(body);

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  EXPECT_EQ(result, ParseState::kInvalid);
  EXPECT_EQ(out, "");
  EXPECT_EQ(body, original);
}

TEST_P(ChunkedDecoderTest, ChunkExtensions) {
  std::string_view body =
      "9;key=value\r\n"
      "pixielabs\r\n"
      "C;why=noidea\r\n"
      " is awesome!\r\n"
      "0;onzero=too\r\n"
      "\r\n";
  std::string original(body);

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  EXPECT_EQ(result, ParseState::kSuccess);
  EXPECT_EQ(out, "pixielabs is awesome!");
  EXPECT_EQ(body, "");
}

TEST_P(ChunkedDecoderTest, Trailers) {
  std::string_view body =
      "9\r\n"
      "pixielabs\r\n"
      "C\r\n"
      " is awesome!\r\n"
      "0\r\n"
      "Trailer: abcd\r\n"
      "\r\n";
  std::string original(body);

  std::string out;
  size_t body_size;
  ParseState result = ParseChunked(&body, kBodySizeLimitBytes, &out, &body_size);

  EXPECT_EQ(result, ParseState::kSuccess);
  EXPECT_EQ(out, "pixielabs is awesome!");
  EXPECT_EQ(body, "");
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
