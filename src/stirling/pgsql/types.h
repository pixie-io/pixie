#pragma once

#include <string>

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
enum class MessageType : char {
  kAuth = 'R',
  kKey = 'K',
  kBind = 'B',
  kBindComplete = '2',
  kClose = 'C',
  kCloseComplete = '3',
  // This is same as kClose, but this is always sent by a server.
  kCmdComplete = 'C',
  kCopy = 'd',
  kCopyDone = 'c',
  kCopyFail = 'f',
  kCopyInResponse = 'G',
  kCopyOutResponse = 'H',
  kQuery = 'Q',
  kReadyForQuery = 'Z',
  kRowDesc = 'T',

  // TODO(yzhao): More tags to be added.
};

/**
 * Regular message's wire format:
 * ---------------------------------------------------------
 * | char tag | int32 len (including this field) | payload |
 * ---------------------------------------------------------
 */
struct RegularMessage {
  char tag;
  int32_t len;
  std::string payload;
};

struct Query : RegularMessage {};

/**
 * Startup message's wire format:
 * -----------------------------------------------------------------------
 * | int32 len (including this field) | int32 protocol version | payload |
 * -----------------------------------------------------------------------
 */
struct StartupMessage {
  int32_t len;
  int32_t protocol;
  std::string payload;
};

struct CancelRequestMessage {
  int32_t len;
  int32_t cancel_code;
  int32_t pid;
  int32_t secret;
};

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
