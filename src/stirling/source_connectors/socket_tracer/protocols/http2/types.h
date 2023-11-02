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

#pragma once

#include <algorithm>
#include <chrono>
#include <map>
#include <string>
#include <utility>

#include <absl/strings/str_join.h>

#include "src/common/base/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http2 {

namespace headers {

constexpr char kContentType[] = "content-type";
constexpr char kMethod[] = ":method";
constexpr char kPath[] = ":path";
constexpr char kGRPCEncoding[] = "grpc-encoding";

constexpr char kContentTypeGRPC[] = "application/grpc";
constexpr char kGZip[] = "gzip";

}  // namespace headers

// Note that NVMap keys (HTTP2 header field names) are assumed to be lowercase to match spec:
//
// From https://http2.github.io/http2-spec/#HttpHeaders:
// ... header field names MUST be converted to lowercase prior to their encoding in HTTP/2.
// A request or response containing uppercase header field names MUST be treated as malformed.
//
// TODO(yzhao): Change to hold a std::map<> object instead of deriving from std::multiplemap<>.
class NVMap : public std::multimap<std::string, std::string> {
 public:
  std::string ValueByKey(const std::string& key, const std::string& default_value = "") const {
    const auto iter = find(key);
    if (iter != end()) {
      return iter->second;
    }
    return default_value;
  }

  size_t ByteSize() const {
    size_t byte_size = 0;
    for (const auto& [name, value] : *this) {
      byte_size += name.size();
      byte_size += value.size();
    }
    return byte_size;
  }

  bool HasKey(const std::string& key) const { return find(key) != end(); }

  std::string ToString() const { return absl::StrJoin(*this, ", ", absl::PairFormatter(":")); }
};

// This struct represents the frames of interest transmitted on an HTTP2 stream.
// It is called a HalfStream because it captures one direction only.
// For example, the request is one HalfStream while the response is on another HalfStream,
// both of which are on the same stream ID of the same connection.
struct HalfStream {
 public:
  const std::string& data() const { return data_; }
  std::string* mutable_data() { return &data_; }
  const NVMap& headers() const { return headers_; }
  NVMap* mutable_headers() { return &headers_; }
  const NVMap& trailers() const { return trailers_; }
  bool end_stream() const { return end_stream_; }
  bool data_truncated() const { return data_truncated_; }
  size_t original_data_size() const { return original_data_size_; }

  // After calling ConsumeData(), the HalfStream is no longer valid.
  // ByteSize() and other calls may be wrong.
  std::string ConsumeData() { return std::move(data_); }

  void UpdateTimestamp(uint64_t t) {
    if (timestamp_ns == 0) {
      timestamp_ns = t;
      bpf_timestamp_ns = t;
    } else {
      timestamp_ns = std::min<uint64_t>(timestamp_ns, t);
      bpf_timestamp_ns = std::min<uint64_t>(bpf_timestamp_ns, t);
    }
  }

  void AddHeader(std::string key, std::string val) {
    byte_size_ += key.size() + val.size();
    headers_.emplace(std::move(key), std::move(val));
  }

  void AddTrailer(std::string key, std::string val) {
    byte_size_ += key.size() + val.size();
    trailers_.emplace(std::move(key), std::move(val));
  }

  void AddData(std::string_view val) {
    // Only sample the head of the body, to save space.
    constexpr size_t kMaxBodyBytes = 512;

    original_data_size_ += val.size();

    size_t size_to_add = val.size();

    if (size_to_add + data_.size() > kMaxBodyBytes) {
      size_to_add = kMaxBodyBytes - data_.size();
      data_truncated_ = true;
    }

    if (size_to_add > 0) {
      byte_size_ += size_to_add;
      data_ += val.substr(0, size_to_add);
    }
  }

  void AddEndStream() { end_stream_ = true; }

  size_t ByteSize() const { return byte_size_; }

  bool HasGRPCContentType() const {
    return absl::StrContains(headers_.ValueByKey(headers::kContentType), headers::kContentTypeGRPC);
  }

  bool HasGZipGRPCEncoding() const {
    return absl::StrContains(headers_.ValueByKey(headers::kGRPCEncoding), headers::kGZip);
  }

  std::string ToString() const {
    return absl::Substitute(
        "[headers=$0 data=$1 trailers=$2 end_stream=$3 byte_size=$4 original_data_size=$5 "
        "data_truncated=$6 ts_ns=$7 bpf_ts_ns=$8]",
        headers_.ToString(), BytesToString<bytes_format::HexAsciiMix>(data_), trailers_.ToString(),
        end_stream_, byte_size_, original_data_size_, data_truncated_, timestamp_ns,
        bpf_timestamp_ns);
  }

  // Timestamp initially assigned from BPF runtime and later adjusted to wall time clock.
  uint64_t timestamp_ns = 0;

  // Timestamp set in the BPF runtime.
  // TODO(yzhao): Remove this after negative latency is not showing anymore.
  uint64_t bpf_timestamp_ns = 0;

 private:
  NVMap headers_;
  std::string data_;
  NVMap trailers_;
  bool end_stream_ = false;
  size_t byte_size_ = 0;

  // Record the size of data (excluding headers), which used for truncation.
  size_t original_data_size_ = 0;
  // If true, means data has been discarded to stay within the limit.
  bool data_truncated_ = false;
};

// This class represents an HTTP2 stream (https://http2.github.io/http2-spec/#StreamsLayer).
// It is split out into a send and recv. Depending on whether we are tracing the requestor
// or the responder, send and recv contain either the request or response.
struct Stream {
  HalfStream send;
  HalfStream recv;

  bool StreamEnded() const {
    // This check only applies to unary RPC calls.
    // In streaming, however, the end_stream is only sent when client or server explicitly indicates
    // so. And the other side can end the RPC without sending a frame with end_stream set to true.
    return send.end_stream() && recv.end_stream();
  }

  bool HasGRPCContentType() const { return send.HasGRPCContentType() || recv.HasGRPCContentType(); }
  bool HasGRPCEncodingHeader() const {
    return send.headers().HasKey(headers::kGRPCEncoding) ||
           recv.headers().HasKey(headers::kGRPCEncoding);
  }
  bool HasGZipGRPCEncoding() const {
    return send.HasGZipGRPCEncoding() || recv.HasGZipGRPCEncoding();
  }

  bool consumed = false;

  size_t ByteSize() const { return send.ByteSize() + recv.ByteSize(); }

  std::string ToString() const {
    return absl::Substitute("[consumed=$0] [send=$1] [recv=$2]", consumed, send.ToString(),
                            recv.ToString());
  }

  // Returns true if both send and recv half streams have headers. AFAIK, HTTP2 client and server
  // always send some headers frame.
  bool HasHeaders() const { return !send.headers().empty() && !recv.headers().empty(); }
};

using Record = Stream;
using stream_id_t = uint16_t;

struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Stream;
  using record_type = Record;
  using state_type = NoState;
  using key_type = stream_id_t;

  static void ConvertTimestamps(record_type* record, ConvertTimestampsFuncType func) {
    record->send.timestamp_ns = func(record->send.timestamp_ns);
    record->recv.timestamp_ns = func(record->recv.timestamp_ns);
  }
};

}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace px
