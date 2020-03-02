#pragma once

#include "src/stirling/cql/types.h"
#include "src/stirling/http/types.h"
#include "src/stirling/http2/http2.h"
#include "src/stirling/http2u/types.h"
#include "src/stirling/mysql/types.h"

namespace pl {
namespace stirling {

/**
 * A set of traits per protocol.
 * Key is the protocol RecordType.
 * Values include FrameType, StateType, etc.
 *
 * Example usage:
 *   ProtocolTraits<mysql::Record>::type --> mysql::Packet
 *
 * @tparam TRecordType The higher-level entry type, which is the map 'key'.
 */

// Note: Can optionally use a state_type of std::monostate to indicate that there is no state.
// The connection tracker understands not to create state object for std::monostate.

namespace http {
struct ProtocolTraits {
  using frame_type = http::Message;
  using record_type = http::Record;
  using state_type = std::monostate;
};
}  // namespace http

namespace http2 {
struct ProtocolTraits {
  using frame_type = http2::Frame;
  using record_type = http2::Record;
  using state_type = http2::State;
};
}  // namespace http2

namespace http2u {
struct ProtocolTraits {
  using frame_type = http2u::Stream;
  using record_type = http2u::Record;
  using state_type = std::monostate;
};
}  // namespace http2u

namespace mysql {
struct ProtocolTraits {
  using frame_type = mysql::Packet;
  using record_type = mysql::Record;
  using state_type = mysql::State;
};
}  // namespace mysql

namespace cass {
struct ProtocolTraits {
  using frame_type = cass::Frame;
  using record_type = cass::Record;
  using state_type = std::monostate;
};
}  // namespace cass

}  // namespace stirling
}  // namespace pl
