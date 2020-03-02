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
template <class TRecordType>
struct ProtocolTraits;

// Note: Can optionally use a state_type of std::monostate to indicate that there is no state.
// The connection tracker understands not to create state object for std::monostate.

template <>
struct ProtocolTraits<http::Record> {
  using frame_type = http::Message;
  using state_type = http::State;
};

template <>
struct ProtocolTraits<http2::Record> {
  using frame_type = http2::Frame;
  using state_type = http2::State;
};

template <>
struct ProtocolTraits<http2u::Record> {
  using frame_type = http2u::Stream;
  using state_type = http2u::State;
};

template <>
struct ProtocolTraits<mysql::Record> {
  using frame_type = mysql::Packet;
  using state_type = mysql::State;
};

template <>
struct ProtocolTraits<cass::Record> {
  using frame_type = cass::Frame;
  using state_type = cass::State;
};

}  // namespace stirling
}  // namespace pl
