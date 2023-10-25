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

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <magic_enum.hpp>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase.

namespace px {
namespace stirling {
namespace protocols {
namespace dns {

//-----------------------------------------------------------------------------
// DNS Frame
//-----------------------------------------------------------------------------

struct DNSHeader {
  uint16_t txid = 0;
  uint16_t flags = 0;
  uint16_t num_queries = 0;
  uint16_t num_answers = 0;
  uint16_t num_auth = 0;
  uint16_t num_addl = 0;
};

constexpr int kTXIDOffset = 0;
constexpr int kFlagsOffset = 2;
constexpr int kNumQueriesOffset = 4;
constexpr int kNumAnswersOffset = 6;
constexpr int kNumAuthOffset = 8;
constexpr int kNumAddlOffset = 10;

// Flags in the DNS header:
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |QR|   Opcode  |AA|TC|RD|RA| Z|AD|CD|   RCODE   |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

#define EXTRACT_DNS_FLAG(flags, pos, width) ((flags >> pos) & ((1 << width) - 1))

constexpr int kQRPos = 15;
constexpr int kOpcodePos = 11;
constexpr int kAAPos = 10;
constexpr int kTCPos = 9;
constexpr int kRDPos = 8;
constexpr int kRAPos = 7;
constexpr int kADPos = 5;
constexpr int kCDPos = 4;
constexpr int kRcodePos = 0;

constexpr int kQRWidth = 1;
constexpr int kOpcodeWidth = 4;
constexpr int kAAWidth = 1;
constexpr int kTCWidth = 1;
constexpr int kRDWidth = 1;
constexpr int kRAWidth = 1;
constexpr int kADWidth = 1;
constexpr int kCDWidth = 1;
constexpr int kRcodeWidth = 4;

// A DNSRecord represents a DNS resource record
// Typically it is the answer to a query (e.g. from name->addr).
// Spec: https://www.ietf.org/rfc/rfc1035.txt
struct DNSRecord {
  std::string name;

  // cname and addr are mutually exclusive.
  // Either a record provdes a cname (an alias to another record), or it resolves the address.
  // TODO(oazizi): Consider using std::variant.
  std::string cname;
  InetAddr addr;
};

struct Frame : public FrameBase {
  DNSHeader header;
  const std::vector<DNSRecord>& records() const { return records_; }
  bool consumed = false;

  void AddRecords(std::vector<DNSRecord>&& records) {
    for (const auto& r : records) {
      records_size_ += r.name.size() + r.cname.size() + sizeof(r.addr);
    }
    records_ = std::move(records);
  }

  size_t ByteSize() const override { return sizeof(Frame) + records_size_; }

 private:
  std::vector<DNSRecord> records_;
  size_t records_size_ = 0;
};

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

struct Request {
  // DNS header (txid, flags, num queries/answers, etc.) as a JSON string.
  std::string header;

  // DNS queries.
  std::string query;

  // Timestamp of the request.
  uint64_t timestamp_ns = 0;

  std::string ToString() const {
    return absl::Substitute("header=$0 query=$1 timestamp_ns=$2", header, query, timestamp_ns);
  }
};

struct Response {
  // DNS header (txid, flags, num queries/answers, etc.) as a JSON string.
  std::string header;

  // Query Answers.
  std::string msg;

  // Timestamp of the response.
  uint64_t timestamp_ns = 0;

  std::string ToString() const {
    return absl::Substitute("header=$0 query=$1 timestamp_ns=$2", header, msg, timestamp_ns);
  }
};

/**
 *  Record is the primary output of the dns parser.
 */
struct Record {
  Request req;
  Response resp;

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

using stream_id_t = uint16_t;
struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = NoState;
  using key_type = stream_id_t;
};

}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace px
