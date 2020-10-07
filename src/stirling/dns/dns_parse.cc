#include "src/stirling/dns/dns_parse.h"

#include <arpa/inet.h>
#include <dnsparser.h>
#include <deque>
#include <memory>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/common/base/inet_utils.h"
#include "src/common/base/types.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/dns/types.h"

namespace pl {
namespace stirling {
namespace dns {

class PxDnsParserListener : public DnsParserListener {
 public:
  void onDnsRec(in_addr addr, std::string name, std::string path) override {
    records_.emplace_back(
        DNSRecord{{SockAddrFamily::kIPv4, addr}, std::move(name), std::move(path)});
  }
  void onDnsRec(in6_addr addr, std::string name, std::string path) override {
    records_.emplace_back(
        DNSRecord{{SockAddrFamily::kIPv6, addr}, std::move(name), std::move(path)});
  }

  std::vector<DNSRecord> records_;
};

ParseState ParseFrame(MessageType type, std::string_view* buf, Frame* result) {
  PL_UNUSED(type);

  PxDnsParserListener response_handler;
  std::unique_ptr<DnsParser> parser = DnsParserNew(&response_handler);
  parser->parse(buf->data(), buf->length());

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
  return std::string::npos;
}

}  // namespace stirling
}  // namespace pl
