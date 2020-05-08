#pragma once

#include <string>
#include <vector>

#include "src/stirling/common/protocol_traits.h"
#include "src/stirling/common/utils.h"

namespace pl {
namespace stirling {
namespace pgsql {

/**
 * List of tags at the beginning of each and every message.
 * The full list can be found at:
 * https://www.postgresql.org/docs/8.2/protocol-message-formats.html
 *
 * NOTE: The enum value names are shortened, which are not exactly what shown in the above document.
 */
enum class Tag : char {
  kAuth = 'R',
  // Password message is response to authenticate request.
  kPasswd = 'p',

  kKey = 'K',
  kBind = 'B',
  kBindComplete = '2',
  kClose = 'C',
  kCloseComplete = '3',
  // This is same as kClose, but this is always sent by a server.
  kCmdComplete = 'C',
  kErrResp = 'E',
  kCopy = 'd',
  kCopyDone = 'c',
  kCopyFail = 'f',
  kCopyInResponse = 'G',
  kCopyOutResponse = 'H',
  kQuery = 'Q',
  kReadyForQuery = 'Z',
  kDataRow = 'D',

  // All these are sent from client. Some tag are duplicate with the above ones.
  kParse = 'P',
  kDesc = 'D',
  kSync = 'S',
  kExecute = 'E',

  // From server.
  kParseComplete = '1',
  kParamDesc = 't',
  kRowDesc = 'T',

  // TODO(yzhao): More tags to be added.
};

/**
 * Regular message's wire format:
 * ---------------------------------------------------------
 * | char tag | int32 len (including this field) | payload |
 * ---------------------------------------------------------
 */
struct RegularMessage : public stirling::FrameBase {
  Tag tag;
  int32_t len;
  std::string payload;

  size_t ByteSize() const override { return 5 + payload.size(); }

  std::string DebugString() const {
    return absl::Substitute("[tag: $0] [len: $1] [payload: $2]", static_cast<char>(tag), len,
                            payload);
  }
};

struct Query : RegularMessage {};

struct NV {
  std::string name;
  std::string value;

  // This allows GoogleTest to print NV values.
  friend std::ostream& operator<<(std::ostream& os, const NV& nv) {
    os << "[" << nv.name << ", " << nv.value << "]";
    return os;
  }
};

/**
 * Startup message's wire format:
 * -----------------------------------------------------------------------
 * | int32 len (including this field) | int32 protocol version | payload |
 * -----------------------------------------------------------------------
 */
struct StartupMessage {
  static constexpr size_t kMinLen = sizeof(int32_t) + sizeof(int32_t);
  int32_t len;
  struct ProtocolVersion {
    int16_t major;
    int16_t minor;
  };
  ProtocolVersion proto_ver;
  // TODO(yzhao): Check if user field is required. See StartupMessage section of
  // https://www.postgresql.org/docs/9.3/protocol-message-formats.html.
  std::vector<NV> nvs;
};

struct CancelRequestMessage {
  int32_t len;
  int32_t cancel_code;
  int32_t pid;
  int32_t secret;
};

struct Record {
  RegularMessage req;
  RegularMessage resp;
};

struct ProtocolTraits {
  using frame_type = RegularMessage;
  using record_type = Record;
  using state_type = NoState;
};

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
