#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <magic_enum.hpp>

#include "src/stirling/common/event_parser.h"  // For FrameBase.
#include "src/stirling/common/protocol_traits.h"

namespace pl {
namespace stirling {
namespace dns {

//-----------------------------------------------------------------------------
// Cassandra Frame
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

// A DNSRecord represents a DNS resource record
// Typically it is the answer to a query (e.g. from name->addr).
// Spec: https://www.ietf.org/rfc/rfc1035.txt
struct DNSRecord {
  InetAddr addr;
  std::string name;
  std::string path;
};

struct Frame : public stirling::FrameBase {
  DNSHeader header;
  std::vector<DNSRecord> records;
  bool consumed = false;

  size_t ByteSize() const override {
    // TODO(oazizi): Update this.
    return sizeof(Frame) + records.size() * sizeof(DNSRecord);
  }
};

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

struct Request {
  // The body of the request, if request has a single string parameter. Otherwise empty for now.
  std::string msg;

  // Timestamp of the request packet.
  uint64_t timestamp_ns = 0;
};

struct Response {
  // Any relevant response message.
  std::string msg;

  // Timestamp of the response packet.
  uint64_t timestamp_ns = 0;
};

/**
 *  Record is the primary output of the mysql parser.
 */
struct Record {
  Request req;
  Response resp;
};

struct ProtocolTraits {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = NoState;
};

}  // namespace dns
}  // namespace stirling
}  // namespace pl
