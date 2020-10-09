#include "src/stirling/protocols/dns/dns_parse.h"

#include <arpa/inet.h>
#include <dnsparser.h>
#include <deque>
#include <memory>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/common/base/inet_utils.h"
#include "src/common/base/types.h"
#include "src/stirling/common/binary_decoder.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/protocols/dns/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace dns {

class PxDnsParserListener : public DnsParserListener {
 public:
  void onDnsRec(in_addr addr, std::string name, std::string path) override {
    records_.emplace_back(
        DNSRecord{{InetAddrFamily::kIPv4, addr}, std::move(name), std::move(path)});
  }
  void onDnsRec(in6_addr addr, std::string name, std::string path) override {
    records_.emplace_back(
        DNSRecord{{InetAddrFamily::kIPv6, addr}, std::move(name), std::move(path)});
  }

  std::vector<DNSRecord> records_;
};

ParseState ParseFrame(MessageType type, std::string_view* buf, Frame* result) {
  PL_UNUSED(type);

  PxDnsParserListener response_handler;
  std::unique_ptr<DnsParser> parser = DnsParserNew(&response_handler);
  int retval = parser->parse(buf->data(), buf->length());
  if (retval == -1) {
    return ParseState::kInvalid;
  }

  // DnsParser ensures there is a complete header.
  DCHECK_GT(buf->size(), sizeof(DNSHeader));
  BinaryDecoder decoder(*buf);
  result->header.txid = decoder.ExtractInt<uint16_t>().ValueOr(0xffff);
  result->header.flags = decoder.ExtractInt<uint16_t>().ValueOr(0xffff);
  result->header.num_queries = decoder.ExtractInt<uint16_t>().ValueOr(0xffff);
  result->header.num_answers = decoder.ExtractInt<uint16_t>().ValueOr(0xffff);
  result->header.num_auth = decoder.ExtractInt<uint16_t>().ValueOr(0xffff);
  result->header.num_addl = decoder.ExtractInt<uint16_t>().ValueOr(0xffff);

  // The ValueOr(0xffff) conditions should never trigger, since there are enough bytes.
  DCHECK_NE(result->header.flags, 0xffff);
  DCHECK_NE(result->header.num_queries, 0xffff);
  DCHECK_NE(result->header.num_answers, 0xffff);
  DCHECK_NE(result->header.num_auth, 0xffff);
  DCHECK_NE(result->header.num_addl, 0xffff);

  result->records = std::move(response_handler.records_);
  buf->remove_prefix(buf->length());

  return ParseState::kSuccess;
}
}  // namespace dns

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, dns::Frame* result) {
  return dns::ParseFrame(type, buf, result);
}

template <>
size_t FindFrameBoundary<dns::Frame>(MessageType /*type*/, std::string_view /*buf*/,
                                     size_t /*start_pos*/) {
  // Not implemented.
  // Search for magic string that we should insert between UDP packets.
  return std::string::npos;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
