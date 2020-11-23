#pragma once

#include <deque>
#include <variant>

#include "src/stirling/protocols/cql/types.h"
#include "src/stirling/protocols/dns/types.h"
#include "src/stirling/protocols/http/types.h"
#include "src/stirling/protocols/http2/types.h"
#include "src/stirling/protocols/mysql/types.h"
#include "src/stirling/protocols/pgsql/types.h"
#include "src/stirling/protocols/redis/types.h"

namespace pl {
namespace stirling {
namespace protocols {

// clang-format off
// PROTOCOL_LIST: Requires update on new protocols.
using FrameDequeVariant = std::variant<std::monostate,
                                       std::deque<cass::Frame>,
                                       std::deque<http::Message>,
                                       std::deque<mysql::Packet>,
                                       std::deque<pgsql::RegularMessage>,
                                       std::deque<dns::Frame>,
                                       std::deque<redis::Message>>;
// clang-format off

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
