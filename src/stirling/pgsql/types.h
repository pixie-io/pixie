#pragma once

#include <deque>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/stirling/common/event_parser.h"
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

// Make Tag printable, for example for DCHECK().
inline std::ostream& operator<<(std::ostream& os, Tag tag) {
  os << static_cast<char>(tag) << ":" << magic_enum::enum_name(tag);
  return os;
}

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

enum class FmtCode : int16_t {
  // https://www.postgresql.org/docs/9.3/protocol-message-formats.html
  kText = 0,
  kBinary = 1,
};

struct Param {
  FmtCode format_code;
  std::optional<std::string> value;
};

struct BindRequest {
  std::string dest_portal_name;
  std::string src_prepared_stat_name;
  std::vector<Param> params;
  std::vector<FmtCode> res_col_fmt_codes;
};

struct ParamDesc {
  std::vector<int32_t> type_oids;
};

struct Parse {
  std::string stmt_name;
  std::string query;
  std::vector<int32_t> param_type_oids;
};

struct RowDesc {
  struct Field {
    std::string_view name;
    // 0 means this field cannot be identified as a column of any table.
    int32_t table_oid;
    // 0 means this field cannot be identified as a column of any table.
    int16_t attr_num;
    int32_t type_oid;
    // Negative values denote variable-width types.
    int16_t type_size;
    int32_t type_modifier;
    FmtCode fmt_code;

    std::string DebugString() const {
      return absl::Substitute(
          "[name=$0 table_oid=$1 attr_num=$2 type_oid=$3 type_size=$4 type_modifier=$5 "
          "fmt_code=$6]",
          name, table_oid, attr_num, type_oid, type_size, type_modifier,
          magic_enum::enum_name(fmt_code));
    }
  };
  std::vector<Field> fields;
};

// See https://www.postgresql.org/docs/9.3/protocol-error-fields.html
// The enum name does not have 'k' prefix, so that they can be used directly.
enum class ErrFieldCode : char {
  Severity = 'S',
  InternalSeverity = 'V',
  Code = 'C',
  Message = 'M',
  Detail = 'D',
  Hint = 'H',
  Position = 'P',
  InternalPosition = 'p',
  InternalQuery = 'q',
  Where = 'W',
  SchemaName = 's',
  TableName = 't',
  ColumnName = 'c',
  DataTypeName = 'd',
  ConstraitName = 'n',
  File = 'F',
  Line = 'L',
  Routine = 'R',
};

struct ErrResp {
  struct Field {
    ErrFieldCode code;
    std::string_view value;
  };
  std::vector<Field> fields;
};

struct ParseReqResp {
  Parse req;
  std::optional<ErrResp> resp;
};

struct Record {
  RegularMessage req;
  RegularMessage resp;
};

struct State {
  absl::flat_hash_map<std::string, std::string> prepared_statements;
  // One postgres session can only have at most one unnamed statement.
  std::string unnamed_statement;
};

struct StateWrapper {
  State global;
  std::monostate send;
  std::monostate recv;
};

struct ProtocolTraits {
  using frame_type = RegularMessage;
  using record_type = Record;
  using state_type = StateWrapper;
};

using MsgDeqIter = std::deque<RegularMessage>::iterator;

struct TagMatcher {
  explicit TagMatcher(std::set<Tag> tags) : target_tags(std::move(tags)) {}
  bool operator()(const RegularMessage& msg) {
    return target_tags.find(msg.tag) != target_tags.end();
  }
  std::set<Tag> target_tags;
};

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
