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

#include "src/stirling/source_connectors/socket_tracer/protocols/dns/parse.h"

#include <arpa/inet.h>
#include <dnsparser/include/dnsparser.h>
#include <cstring>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "src/common/base/byte_utils.h"
#include "src/common/base/inet_utils.h"
#include "src/common/base/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/dns_message.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"
#include "src/stirling/utils/binary_decoder.h"
#include "src/stirling/utils/parse_state.h"

namespace px {
namespace stirling {
namespace protocols {
namespace dns {

class PxDnsParserListener : public DnsParserListener {
 public:
  void onDnsRec(std::string name, in_addr addr) override {
    records_.emplace_back(DNSRecord{std::move(name), "", {InetAddrFamily::kIPv4, addr}});
  }
  void onDnsRec(std::string name, in6_addr addr) override {
    records_.emplace_back(DNSRecord{std::move(name), "", {InetAddrFamily::kIPv6, addr}});
  }
  void onDnsRec(std::string name, std::string cname) override {
    records_.emplace_back(DNSRecord{std::move(name), std::move(cname), {}});
  }

  std::vector<DNSRecord> records_;
};

namespace {

constexpr uint16_t kOpcodeMask = ((1 << kOpcodeWidth) - 1) << kOpcodePos;

void ExtractHeader(std::string_view msg, DNSHeader* header) {
  BinaryDecoder decoder(msg);
  header->txid = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  header->flags = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  header->num_queries = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  header->num_answers = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  header->num_auth = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  header->num_addl = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
}

std::vector<DNSRecord> RecordsFromWalk(DNSMessageInfo* info) {
  std::vector<DNSRecord> records;
  const bool is_response = EXTRACT_DNS_FLAG(info->flags, kQRPos, kQRWidth) == 1;
  if (!is_response) {
    for (auto& q : info->questions) {
      DNSRecord r;
      r.name = std::move(q.name);
      if (q.qtype == kDNSTypeA) {
        r.addr = InetAddr{InetAddrFamily::kIPv4, in_addr{}};
      } else if (q.qtype == kDNSTypeAAAA) {
        r.addr = InetAddr{InetAddrFamily::kIPv6, in6_addr{}};
      } else {
        r.type = DNSTypeName(q.qtype);
      }
      records.push_back(std::move(r));
    }
    return records;
  }
  for (auto& a : info->answers) {
    DNSRecord r;
    r.name = std::move(a.name);
    switch (a.type) {
      case kDNSTypeA: {
        in_addr addr;
        memcpy(&addr, a.addr.data(), sizeof(addr));
        r.addr = InetAddr{InetAddrFamily::kIPv4, addr};
      } break;
      case kDNSTypeAAAA: {
        in6_addr addr;
        memcpy(&addr, a.addr.data(), sizeof(addr));
        r.addr = InetAddr{InetAddrFamily::kIPv6, addr};
      } break;
      case kDNSTypeCNAME:
        r.cname = std::move(a.target);
        break;
      case kDNSTypePTR:
        r.type = "PTR";
        r.ptr = std::move(a.target);
        break;
      default:
        continue;
    }
    records.push_back(std::move(r));
  }
  return records;
}

}  // namespace

ParseState ParseFrame(message_type_t type, std::string_view* buf, Frame* result) {
  PX_UNUSED(type);

  DNSMessageInfo info;
  if (WalkDNSMessage(*buf, &info)) {
    const std::string_view msg = buf->substr(0, info.length);
    buf->remove_prefix(info.length);
    if ((info.flags & kOpcodeMask) != 0) {
      return ParseState::kIgnored;
    }
    ExtractHeader(msg, &result->header);
    result->AddRecords(RecordsFromWalk(&info));
    return ParseState::kSuccess;
  }

  PxDnsParserListener response_handler;
  std::unique_ptr<DnsParser> parser = DnsParserNew(&response_handler);
  int retval = parser->parse(buf->data(), buf->length());
  if (retval == -1) {
    return ParseState::kInvalid;
  }

  // DnsParser ensures there is a complete header.
  CTX_DCHECK_GT(buf->size(), sizeof(DNSHeader));
  ExtractHeader(*buf, &result->header);

  // The ValueOr(0xffff) conditions should never trigger, since there are enough bytes.
  CTX_DCHECK_NE(result->header.flags, 0xffff);
  CTX_DCHECK_NE(result->header.num_queries, 0xffff);
  CTX_DCHECK_NE(result->header.num_answers, 0xffff);
  CTX_DCHECK_NE(result->header.num_auth, 0xffff);
  CTX_DCHECK_NE(result->header.num_addl, 0xffff);

  result->AddRecords(std::move(response_handler.records_));
  buf->remove_prefix(buf->length());

  return ParseState::kSuccess;
}
}  // namespace dns

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, dns::Frame* result,
                      NoState* /*state*/) {
  return dns::ParseFrame(type, buf, result);
}

template <>
size_t FindFrameBoundary<dns::Frame>(message_type_t /*type*/, std::string_view /*buf*/,
                                     size_t /*start_pos*/, NoState* /*state*/) {
  // Not implemented.
  // Search for magic string that we should insert between UDP packets.
  return std::string::npos;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
