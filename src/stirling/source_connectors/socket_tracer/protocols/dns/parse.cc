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
#include <memory>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/common/base/inet_utils.h"
#include "src/common/base/types.h"
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

ParseState ParseFrame(message_type_t type, std::string_view* buf, Frame* result) {
  PX_UNUSED(type);

  PxDnsParserListener response_handler;
  std::unique_ptr<DnsParser> parser = DnsParserNew(&response_handler);
  int retval = parser->parse(buf->data(), buf->length());
  if (retval == -1) {
    return ParseState::kInvalid;
  }

  // DnsParser ensures there is a complete header.
  CTX_DCHECK_GT(buf->size(), sizeof(DNSHeader));
  BinaryDecoder decoder(*buf);
  result->header.txid = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  result->header.flags = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  result->header.num_queries = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  result->header.num_answers = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  result->header.num_auth = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);
  result->header.num_addl = decoder.ExtractBEInt<uint16_t>().ValueOr(0xffff);

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
