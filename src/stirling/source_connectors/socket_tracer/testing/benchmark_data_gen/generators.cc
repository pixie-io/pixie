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

#include "src/stirling/source_connectors/socket_tracer/testing/benchmark_data_gen/generators.h"

namespace px {
namespace stirling {
namespace testing {

RecordGenerator::Record SingleReqRespGen::Next(int32_t) {
  RecordGenerator::Record record;
  record.frames.emplace_back(kIngress, req_bytes_);
  record.recv_bytes = req_bytes_.size();
  record.frames.emplace_back(kEgress, resp_bytes_);
  record.send_bytes = resp_bytes_.size();
  return record;
}

HTTP1SingleReqRespGen::HTTP1SingleReqRespGen(size_t total_size, size_t chunk_size, char c)
    : SingleReqRespGen(kDefaultHTTPReq, "") {
  size_t remaining = total_size;
  remaining -= req_bytes_.size();
  remaining -= (kDefaultHTTPRespFmt.size() - std::string_view("$0$1").size());

  std::string additional_headers;
  std::string body;
  if (chunk_size != 0) {
    additional_headers = "Transfer-Encoding: chunked\r\n";
    remaining -= additional_headers.size();
    std::string end_chunk = "0\r\n\r\n";
    remaining -= end_chunk.size();

    std::string chunk_hdr = absl::StrCat(absl::Hex(chunk_size), "\r\n");
    size_t chunk_overhead = chunk_hdr.size() + std::string_view("\r\n").size();
    size_t chunk_size_w_overhead = chunk_size + chunk_overhead;
    size_t num_chunks = remaining / chunk_size_w_overhead;
    for (size_t i = 0; i < num_chunks; ++i) {
      absl::StrAppend(&body, chunk_hdr, std::string(chunk_size, c), "\r\n");
    }
    remaining -= num_chunks * chunk_size_w_overhead;
    if (remaining > chunk_overhead) {
      remaining -= chunk_overhead;
      chunk_hdr = absl::StrCat(absl::Hex(remaining), "\r\n");
      absl::StrAppend(&body, chunk_hdr, std::string(remaining, c), "\r\n");
    }
    absl::StrAppend(&body, end_chunk);
  } else {
    std::string_view content_length_hdr_fmt = "Content-Length: $0\r\n";
    remaining -= content_length_hdr_fmt.size();
    // Estimate the length of the content string based on remaining before the subtraction.
    remaining -= (absl::StrCat(remaining).size() - std::string("$0").size());
    additional_headers = absl::Substitute(content_length_hdr_fmt, remaining);
    body = std::string(remaining, c);
  }

  resp_bytes_ = absl::Substitute(kDefaultHTTPRespFmt, additional_headers, body);
}

uint64_t NoGapsPosGenerator::NextPos(uint64_t msg_size) {
  uint64_t ret = pos_;
  pos_ += msg_size;
  return ret;
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
