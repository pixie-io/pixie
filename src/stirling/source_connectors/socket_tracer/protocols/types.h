#pragma once

#include <deque>
#include <variant>

#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/types.h"

namespace px {
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
}  // namespace px
