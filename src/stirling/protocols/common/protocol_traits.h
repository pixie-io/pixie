#pragma once

#include <variant>

namespace pl {
namespace stirling {

// Each protocol should define a struct called defining its protocol traits.
// This ProtocolTraits struct should define the following types:
// - frame_type: This is the low-level frame to which the raw data is parsed.
//               Examples: http::Message, cql::Frame, mysql::Packet
// - state_type: This is state struct that contains any relevant state for the protocol.
//               The state_type must have three members: global, send and recv.
//               A convenience NoState struct is defined for any protocols that have no state.
// - record_type: This is the request response pair, the content of which has been interpreted.
//                This struct will be passed to the SocketTraceConnector to be appended to the
//                appropriate table.
//
// Example for HTTP protocol:
//
// namespace http {
// struct ProtocolTraits {
//   using frame_type = Message;
//   using record_type = Record;
//   using state_type = NoState;
// };
// }
//
// Note that the ProtocolTraits are hooked into the SocketTraceConnector through the
// protocol_transfer_specs.

// A default state implementation, provided for convenience.
// Setting ProtocolTraits::state_type to NoState indicate that there is no state for the protocol.
// As an optimization, the connection tracker understands not to create state object for NoState.
struct NoState {
  std::monostate global;
  std::monostate send;
  std::monostate recv;
};

}  // namespace stirling
}  // namespace pl
