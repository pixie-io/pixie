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

#include "src/stirling/source_connectors/socket_tracer/protocols/http/body_decoder.h"

#include <picohttpparser.h>

#include <algorithm>
#include <utility>
#include <vector>

DEFINE_bool(use_pico_chunked_decoder, false,
            "If true, uses picohttpparser's chunked decoder; otherwise uses our custom decoder.");

namespace px {
namespace stirling {
namespace protocols {
namespace http {

namespace {

// Length of the CRLF delimiter in HTTP.
constexpr int kDelimiterLen = 2;

/**
 * Extracts the HTTP chunk header, which encodes the chunk length and an optional chunk extension.
 *
 * Examples:
 *    9\r\n             <--- Returns 9
 *    1F\r\n            <--- Returns 31
 *    9;key=value\r\n   <--- Returns 9; This example shows the concept called chunk extensions.
 *
 * @param data Data buffer of the HTTP chunked-encoding message body. The byte of this string_view
 *             are consumed as they are processed
 * @param out A pointer to a variable where the parsed length will be written.
 * @return ParseState::kInvalid if message is malformed.
 *         ParseState::kNeedsMoreData if the message is incomplete.
 *         ParseState::kSuccess if the chunk length was extracted and chunk header is well-formed.
 */
ParseState ExtractChunkLength(std::string_view* data, size_t* out) {
  size_t chunk_len = 0;

  // Maximum number of hex characters we allow in a chunked length encoding.
  // Choosing a large number to account for chunk extensions.
  // HTTP protocol does not specify a size limit for these, but we set a practical limit.
  // Note that HTTP servers do similar things for HTTP headers
  // (e.g. Apache sets an 8K limit for headers).
  constexpr int kSearchWindow = 2048;

  size_t delimiter_pos = data->substr(0, kSearchWindow).find("\r\n");
  if (delimiter_pos == data->npos) {
    return data->length() > kSearchWindow ? ParseState::kInvalid : ParseState::kNeedsMoreData;
  }

  std::string_view chunk_len_str = data->substr(0, delimiter_pos);

  // Remove chunk extensions if present.
  size_t chunk_ext_pos = chunk_len_str.find(";");
  if (chunk_ext_pos != chunk_len_str.npos) {
    chunk_len_str = chunk_len_str.substr(0, chunk_ext_pos);
  }

  bool success = absl::SimpleHexAtoi(chunk_len_str, &chunk_len);
  if (!success) {
    return ParseState::kInvalid;
  }

  data->remove_prefix(delimiter_pos + kDelimiterLen);

  *out = chunk_len;
  return ParseState::kSuccess;
}

/**
 * Extracts the HTTP chunk data, given the chunk length.
 *
 * @param data Data buffer of the HTTP chunked-encoding message body starting at the chunk data
 *             (chunk length should have already been removed).
 *             The byte of this string_view are consumed as they are processed
 * @param out A string_view to the chunk contents that will be set upon success.
 * @return ParseState::kInvalid if message is malformed.
 *         ParseState::kNeedsMoreData if the message is incomplete.
 *         ParseState::kSuccess if the chunk data was extracted and chunk data was well-formed.
 */
ParseState ExtractChunkData(std::string_view* data, size_t chunk_len, std::string_view* out) {
  std::string_view chunk_data;

  if (data->length() < chunk_len + kDelimiterLen) {
    return ParseState::kNeedsMoreData;
  }

  chunk_data = data->substr(0, chunk_len);

  data->remove_prefix(chunk_len);

  // Expect a \r\n to terminate the data chunk.
  if ((*data)[0] != '\r' || (*data)[1] != '\n') {
    return ParseState::kInvalid;
  }

  data->remove_prefix(kDelimiterLen);

  *out = chunk_data;
  return ParseState::kSuccess;
}
}  // namespace

// This is an alternative to the picohttpparser implementation,
// because that one is destructive on incomplete data.
// We may attempt parsing in the middle of a stream and cannot
// have both the result fail and the input buffer be modified.
// Reference: https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
ParseState CustomParseChunked(std::string_view* buf, size_t body_size_limit_bytes,
                              std::string* result, size_t* body_size) {
  std::vector<std::string_view> chunks;
  size_t total_bytes = 0;

  std::string_view data = *buf;

  ParseState s;

  while (true) {
    // Extract the chunk length.
    size_t chunk_len = 0;
    s = ExtractChunkLength(&data, &chunk_len);
    if (s != ParseState::kSuccess) {
      return s;
    }

    // A length of zero marks the end of data.
    if (chunk_len == 0) {
      break;
    }

    // Extract the chunk data.
    std::string_view chunk_data;
    s = ExtractChunkData(&data, chunk_len, &chunk_data);
    if (s != ParseState::kSuccess) {
      return s;
    }

    // Only bother collecting chunks up to a certain size, since we will truncate anyways.
    // Don't break out of the parsing though, since we need to know where the body ends.
    if (total_bytes + chunk_data.size() < body_size_limit_bytes) {
      chunks.push_back(chunk_data);
    } else if (total_bytes < body_size_limit_bytes) {
      size_t bytes_available = body_size_limit_bytes - total_bytes;
      chunks.push_back(chunk_data.substr(0, bytes_available));
    }

    total_bytes += chunk_data.size();
  }

  // Two scenarios to wrap up:
  //   No trailers (common case): Immediately expect one more \r\n
  //   Trailers: End on next \r\n\r\n.
  if (data.length() >= kDelimiterLen && data[0] == '\r' && data[1] == '\n') {
    data.remove_prefix(kDelimiterLen);
  } else {
    // HTTP doesn't specify a limit on how big headers and trailers can be.
    // 8K is the maximum headers size in many popular HTTP servers (like Apache),
    // so use that as a proxy of the maximum trailer size we can expect.
    constexpr int kSearchWindow = 8192;

    size_t pos = data.substr(0, kSearchWindow).find("\r\n\r\n");
    if (pos == data.npos) {
      return data.length() > kSearchWindow ? ParseState::kInvalid : ParseState::kNeedsMoreData;
    }

    data.remove_prefix(pos + 4);
  }

  *result = absl::StrJoin(chunks, "");
  *body_size = total_bytes;

  // Update the input buffer only if the data was parsed properly, because
  // we don't want to be destructive on failure.
  *buf = data;
  return ParseState::kSuccess;
}

// Parse an HTTP chunked body using pico's parser. This implementation
// has the disadvantage that it incurs a potentially expensive copy even when
// the final result is kNeedsMoreData.
// See our Custom implementation for an alternative that doesn't have that cost.
ParseState PicoParseChunked(std::string_view* data, size_t body_size_limit_bytes,
                            std::string* result, size_t* body_size) {
  // Make a copy of the data because phr_decode_chunked mutates the input,
  // and if the original parse fails due to a lack of data, we need the original
  // state to be preserved.
  std::string data_copy(*data);

  phr_chunked_decoder chunk_decoder = {};
  chunk_decoder.consume_trailer = 1;
  char* buf = data_copy.data();
  size_t buf_size = data_copy.size();
  ssize_t retval = phr_decode_chunked(&chunk_decoder, buf, &buf_size);

  if (retval == -1) {
    // Parse failed.
    return ParseState::kInvalid;
  } else if (retval == -2) {
    // Incomplete message.
    return ParseState::kNeedsMoreData;
  } else if (retval >= 0) {
    // Found a complete message.
    data_copy.resize(std::min(buf_size, body_size_limit_bytes));
    data_copy.shrink_to_fit();
    *result = std::move(data_copy);
    *body_size = buf_size;

    // phr_decode_chunked rewrites the buffer in place, removing chunked-encoding headers.
    // So we cannot simply remove the prefix, but rather have to shorten the buffer too.
    // This is done via retval, which specifies how many unprocessed bytes are left.
    data->remove_prefix(data->size() - retval);

    return ParseState::kSuccess;
  }

  LOG(DFATAL) << "Unexpected retval from phr_decode_chunked()";
  return ParseState::kUnknown;
}

// Parse an HTTP message body in the chunked transfer-encoding.
// Reference: https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
ParseState ParseChunked(std::string_view* data, size_t body_size_limit_bytes, std::string* result,
                        size_t* body_size) {
  return (FLAGS_use_pico_chunked_decoder)
             ? PicoParseChunked(data, body_size_limit_bytes, result, body_size)
             : CustomParseChunked(data, body_size_limit_bytes, result, body_size);
}

ParseState ParseContent(std::string_view content_len_str, std::string_view* data,
                        size_t body_size_limit_bytes, std::string* result, size_t* body_size) {
  size_t len;
  if (!absl::SimpleAtoi(content_len_str, &len)) {
    LOG(ERROR) << absl::Substitute("Unable to parse Content-Length: $0", content_len_str);
    return ParseState::kInvalid;
  }

  if (data->size() < len) {
    return ParseState::kNeedsMoreData;
  }

  *result = data->substr(0, std::min(len, body_size_limit_bytes));
  *body_size = len;
  data->remove_prefix(std::min(len, data->size()));
  return ParseState::kSuccess;
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
