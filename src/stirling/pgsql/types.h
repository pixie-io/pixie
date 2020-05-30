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
 * List of tags at the beginning of each and every RegularMessage.
 *
 * The full list can be found at:
 * https://www.postgresql.org/docs/8.2/protocol-message-formats.html
 *
 * Use the notation in the above webpage, F refers to a tag used by messages sent by the Frontend,
 * B Backend.
 *
 * NOTE: The enum value names are shortened, which are not exactly what shown in the above document.
 */
enum class Tag : char {
  // Frontend (F) & Backend (B).
  kCopyData = 'd',
  kCopyDone = 'c',

  // F.
  kDataRow = 'D',
  kQuery = 'Q',
  kCopyFail = 'f',
  kClose = 'C',
  kBind = 'B',
  kPasswd = 'p',
  kParse = 'P',
  kDesc = 'D',
  kSync = 'S',
  kExecute = 'E',

  // B.
  kReadyForQuery = 'Z',
  kCopyOutResponse = 'H',
  kCopyInResponse = 'G',
  kErrResp = 'E',
  kCmdComplete = 'C',
  kCloseComplete = '3',
  kBindComplete = '2',
  kKey = 'K',
  kAuth = 'R',
  kParseComplete = '1',
  kParamDesc = 't',
  kRowDesc = 'T',
  kNoData = 'n',

  kUnknown = '\0',

  // TODO(yzhao): This list is not complete. More tags to be added.
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
  Tag tag = Tag::kUnknown;
  int32_t len = 0;
  std::string payload;

  size_t ByteSize() const override { return 5 + payload.size(); }

  std::string DebugString() const {
    return absl::Substitute("[tag=$0 len=$1 payload=$2]", static_cast<char>(tag), len, payload);
  }
};

template <typename TValue>
struct DebugStringFormatter {
  void operator()(std::string* out, const TValue& v) const { out->append(v.DebugString()); }
};

/**
 * Startup message's wire format:
 * -----------------------------------------------------------------------
 * | int32 len (including this field) | int32 protocol version | payload |
 * -----------------------------------------------------------------------
 *
 * NOTE: StartupMessage is sent from client as the first message.
 * It is not same as RegularMessage, and has no tag.
 */
struct StartupMessage {
  struct ProtocolVersion {
    int16_t major;
    int16_t minor;
  };

  static constexpr size_t kMinLen = sizeof(int32_t) + sizeof(int32_t);

  int32_t len = 0;
  ProtocolVersion proto_ver;
  // TODO(yzhao): Check if user field is required. See StartupMessage section of
  // https://www.postgresql.org/docs/9.3/protocol-message-formats.html.
  std::vector<NV> nvs;
};

struct CancelRequestMessage {
  uint64_t timestamp_ns = 0;

  int32_t len = 0;
  int32_t cancel_code = 0;
  int32_t pid = 0;
  int32_t secret = 0;
};

enum class FmtCode : int16_t {
  // https://www.postgresql.org/docs/9.3/protocol-message-formats.html
  kText = 0,
  kBinary = 1,
};

struct Param {
  FmtCode format_code = FmtCode::kText;
  std::optional<std::string> value;

  std::string_view Value() const {
    // TODO(PP-1885): Format value based on format_code.
    if (value.has_value()) {
      return value.value();
    }
    return "[NULL]";
  }

  std::string DebugString() const {
    return absl::Substitute("[formt=$0 value=$1]", magic_enum::enum_name(format_code), Value());
  }
};

inline bool operator==(const Param& lhs, const Param& rhs) {
  return std::tie(lhs.format_code, lhs.value) == std::tie(rhs.format_code, rhs.value);
}

struct BindRequest {
  uint64_t timestamp_ns = 0;

  // Portal is for maintaining cursored query:
  // https://www.postgresql.org/docs/9.2/plpgsql-cursors.html
  std::string dest_portal_name;
  std::string src_prepared_stat_name;
  std::vector<Param> params;
  std::vector<FmtCode> res_col_fmt_codes;

  struct FmtCodeFormatter {
    void operator()(std::string* out, FmtCode fmt_code) const {
      out->append(magic_enum::enum_name(fmt_code));
    }
  };

  std::string DebugString() const {
    return absl::Substitute("[portal=$0 statement=$1 parameters=[$2] result_format_codes=[$3]]",
                            dest_portal_name, src_prepared_stat_name,
                            absl::StrJoin(params, ", ", DebugStringFormatter<Param>()),
                            absl::StrJoin(res_col_fmt_codes, ", ", FmtCodeFormatter()));
  }
};

struct ParamDesc {
  uint64_t timestamp_ns = 0;

  std::vector<int32_t> type_oids;
};

struct Parse {
  uint64_t timestamp_ns = 0;

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

  uint64_t timestamp_ns = 0;

  std::vector<Field> fields;

  auto FieldNames() const {
    std::vector<std::string_view> result;
    for (const auto f : fields) {
      result.push_back(f.name);
    }
    return result;
  }

  std::string DebugString() const {
    return absl::StrJoin(fields, ", ", DebugStringFormatter<Field>());
  }
};

struct Desc {
  enum class Type : char {
    kStatement = 'S',
    kPortal = 'P',
  };

  uint64_t timestamp_ns = 0;

  Type type = Type::kStatement;
  std::string name;

  std::string DebugString() const {
    return absl::Substitute("[type=$0 name=$1", magic_enum::enum_name(type), name);
  }
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

  kUnknown = '\0',
};

struct ErrResp {
  struct Field {
    ErrFieldCode code = ErrFieldCode::kUnknown;
    std::string_view value;

    std::string DebugString() const {
      std::string_view field_name = magic_enum::enum_name(code);
      if (field_name.empty()) {
        std::string unknown_field_name = "Field:";
        unknown_field_name.append(1, static_cast<char>(code));
        return absl::StrCat(unknown_field_name, "=", value);
      } else {
        return absl::StrCat(field_name, "=", value);
      }
    }
  };

  uint64_t timestamp_ns = 0;

  std::vector<Field> fields;

  struct ErrFieldFormatter {
    void operator()(std::string* out, const ErrResp::Field& err_field) const {
      out->append(err_field.DebugString());
    }
  };

  std::string DebugString() const { return absl::StrJoin(fields, "\n", ErrFieldFormatter()); }
};

struct ParseReqResp {
  Parse req;
  std::optional<ErrResp> resp;
};

struct BindReqResp {
  BindRequest req;
  std::optional<ErrResp> resp;
};

struct DescReqResp {
  Desc req;

  struct Resp {
    uint64_t timestamp_ns = 0;

    bool is_err_resp = false;
    bool is_no_data = false;

    // This is unset if Desc asks for the description of a "portal".
    ParamDesc param_desc;

    RowDesc row_desc;

    // This is set if is_err_resp is true.
    ErrResp err_resp;

    std::string DebugString() const {
      if (is_err_resp) {
        return err_resp.DebugString();
      }
      // It does not seem useful to format type OIDs, so we only report row description's field
      // names.
      return absl::StrJoin(row_desc.FieldNames(), ",");
    }
  };

  Resp resp;
};

struct Record {
  RegularMessage req;
  RegularMessage resp;
};

struct State {
  absl::flat_hash_map<std::string, std::string> prepared_statements;

  // One postgres session can only have at most one unnamed statement.
  std::string unnamed_statement;

  // The resolved query statement of the current extended query session.
  // https://www.postgresql.org/docs/10/protocol-flow.html#PROTOCOL-FLOW-EXT-QUERY
  std::string bound_statement;
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
