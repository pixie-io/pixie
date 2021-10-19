#pragma once

#include <magic_enum.hpp>
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

enum class Type : int8_t {
    kTreq = 1,
    kRreq = -1,
    kTdispatch = 2,
    kRdispatch = -2,

    // control messages
    kTdrain = 64,
    kRdrain = -64,
    kTping  = 65,
    kRping  = -65,

    kTdiscarded = 66,
    kRdiscarded = -66,

    kTlease = 67,

    kTinit = 68,
    kRinit = -68,

    kRerr = -128,

    // only used to preserve backwards compatibility
    kTdiscardedOld = -62,
    kRerrOld       = 127,
};

inline bool IsMuxType(int8_t t) {
  std::optional<Type> mux_type = magic_enum::enum_cast<Type>(t);
  return mux_type.has_value();
}

inline std::optional<Type> GetMatchingRespType(Type req_type) {
    switch (req_type) {
        case Type::kRerrOld:
            return Type::kRerrOld;
        case Type::kRerr:
            return Type::kRerr;
        case Type::kTinit:
            return Type::kRinit;
        case Type::kTping:
            return Type::kRping;
        case Type::kTreq:
            return Type::kRreq;
        case Type::kTdrain:
            return Type::kRdrain;
        case Type::kTdispatch:
            return Type::kRdispatch;
        case Type::kTdiscardedOld:
        case Type::kTdiscarded:
            return Type::kRdiscarded;
        default:
            LOG(DFATAL) << absl::Substitute("Unexpected request type $0", magic_enum::enum_name(req_type));
            return {};
    }
}

/**
 * The mux protocol is explained in more detail in the finagle source code
 * here (https://github.com/twitter/finagle/blob/release/finagle-mux/src/main/scala/com/twitter/finagle/mux/package.scala)
 *
 * Mux messages can take on a few different wire formats. Each type
 * is described below. All fields are big endian.
 *
 * Regular message
 * ----------------------------------------------
 * | uint32 header size | int8 type | int24 tag |
 * ----------------------------------------------
 * |                 Payload                    |
 * ----------------------------------------------
 *
 * Rinit message
 * ----------------------------------------------
 * | uint32 header size | int8 type | int24 tag |
 * ----------------------------------------------
 * |                   Why                      |
 * ----------------------------------------------
 *
 * Rdispatch / Tdispatch (Tdispatch does not have reply status)
 * ----------------------------------------------
 * | uint32 header size | int8 type | int24 tag |
 * ----------------------------------------------
 * |            uint8 reply status              |
 * ----------------------------------------------
 * | uint16 # context | uint16 ctx key length   |
 * ----------------------------------------------
 * | ctx key          | uint16 ctx value length |
 * ----------------------------------------------
 * | ctx value        | uint16 ctx value length |
 * ----------------------------------------------
 * | uint16 destination length | uint16 # dtabs |
 * ----------------------------------------------
 * | uint16 source len |         source         |
 * ----------------------------------------------
 * | uint16 dest len   |       destination      |
 * ----------------------------------------------
 */
struct Frame : public FrameBase {
  // The length of the mux header and the application protocol data excluding
  // the 4 byte length field. For Tdispatch / Rdispatch messages when using a
  // protocol like thriftmux, this would include the length of the mux and thrift
  // data.
  uint32_t length;
  int8_t type;
  uint32_t tag;
  std::string why;
  // Reply status codes. Only present in Rdispatch messages types
  int8_t reply_status;
  std::map<std::string, std::map<std::string, std::string>> context;

  size_t ByteSize() const override { return length; }

  /*
   * Returns the number of bytes remaining in the mux body / payload
   * after parsing the required fields for all packets: header size,
   * type and tag.
   *
   * Since mux's header size field is not included in the size field
   * this will be 4 bytes less the length member (to account for type
   * and tag fields).
   *
   * This is typically used when reading the rest of the payload for
   * the RerrOld, Rerr, Rinit and Tinit messages that contain a why
   * message, tls or compression parameters, etc.
  */
  size_t MuxBodyLength() const { return length - 4; }

  // TODO(ddelnano): Include printing the context, dtabs and other fields
  std::string ToString() const override {
    return absl::Substitute("Mux message [len=$0 type=$1 tag=$2 # context: TBD dtabs: TBD]", length, type, tag);
  }

};

struct Record {
    Frame req;
    Frame resp;
};

struct ProtocolTraits {
  using frame_type = Frame;
  using record_type = Record;
  // TODO(ddelnano): mux does have state but assume no state for now
  using state_type = NoState;
};

}
}
}
}
