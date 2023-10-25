/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <deque>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {
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
  kQuery = 'Q',
  kCopyFail = 'f',
  kClose = 'C',
  kBind = 'B',
  kPasswd = 'p',
  kParse = 'P',
  kDesc = 'D',
  kSync = 'S',
  kExecute = 'E',
  kTerminate = 'X',

  // B.
  kEmptyQueryResponse = 'I',
  kDataRow = 'D',
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
  kParamStatus = 'S',
  kRowDesc = 'T',
  kNoData = 'n',

  kUnknown = '\0',

  // TODO(yzhao): This list is not complete. More tags to be added.
};

// Make Tag printable, for example for CTX_DCHECK().
inline std::ostream& operator<<(std::ostream& os, Tag tag) {
  os << static_cast<char>(tag) << ":" << magic_enum::enum_name(tag);
  return os;
}

inline std::string ToString(Tag tag, bool is_req) {
#define TAG_CASE(tag, name) \
  case tag:                 \
    return name;

  if (is_req) {
    switch (tag) {
      TAG_CASE(Tag::kCopyData, "Copy Data")
      TAG_CASE(Tag::kCopyDone, "Copy Done")
      TAG_CASE(Tag::kQuery, "Query")
      TAG_CASE(Tag::kCopyFail, "Copy Fail")
      TAG_CASE(Tag::kClose, "Close")
      TAG_CASE(Tag::kBind, "Bind")
      TAG_CASE(Tag::kPasswd, "Password Message")
      TAG_CASE(Tag::kParse, "Parse")
      TAG_CASE(Tag::kDesc, "Describe")
      TAG_CASE(Tag::kSync, "Sync")
      TAG_CASE(Tag::kExecute, "Execute")
      TAG_CASE(Tag::kTerminate, "Terminate")
      TAG_CASE(Tag::kUnknown, "Unknown")
      default:
        CTX_DCHECK(false) << "Request tag " << magic_enum::enum_name(tag)
                          << " has no ToString case";
        return "Unknown";
    }
  } else {
    switch (tag) {
      TAG_CASE(Tag::kEmptyQueryResponse, "Empty Query Response")
      TAG_CASE(Tag::kCopyData, "Copy Data")
      TAG_CASE(Tag::kCopyDone, "Copy Done")
      TAG_CASE(Tag::kDataRow, "Data Row")
      TAG_CASE(Tag::kReadyForQuery, "Ready For Query")
      TAG_CASE(Tag::kCopyOutResponse, "Copy Out Response")
      TAG_CASE(Tag::kCopyInResponse, "Copy In Response")
      TAG_CASE(Tag::kErrResp, "Error Response")
      TAG_CASE(Tag::kCmdComplete, "Command Complete")
      TAG_CASE(Tag::kCloseComplete, "Close Complete")
      TAG_CASE(Tag::kBindComplete, "Bind Complete")
      TAG_CASE(Tag::kKey, "Backend Key Data")
      TAG_CASE(Tag::kAuth, "Authentication")
      TAG_CASE(Tag::kParseComplete, "Parse Complete")
      TAG_CASE(Tag::kParamDesc, "Parameter Description")
      TAG_CASE(Tag::kParamStatus, "Parameter Status")
      TAG_CASE(Tag::kRowDesc, "Row Description")
      TAG_CASE(Tag::kNoData, "No Data")
      TAG_CASE(Tag::kUnknown, "Unknown")
      default:
        CTX_DCHECK(false) << "Response tag " << magic_enum::enum_name(tag)
                          << " has no ToString case";
        return "Unknown";
    }
  }
#undef TAG_CASE
}

/**
 * Regular message's wire format:
 * ---------------------------------------------------------
 * | char tag | int32 len (including this field) | payload |
 * ---------------------------------------------------------
 */
struct RegularMessage : public FrameBase {
  Tag tag = Tag::kUnknown;
  int32_t len = 0;
  std::string payload;
  bool consumed = false;

  size_t ByteSize() const override { return 5 + payload.size(); }

  std::string ToString() const override {
    return absl::Substitute("REGULAR MESSAGE [tag=$0 len=$1 payload=$2]", static_cast<char>(tag),
                            len, payload);
  }
};

template <typename TValue>
struct ToStringFormatter {
  void operator()(std::string* out, const TValue& v) const { out->append(v.ToString()); }
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

  std::string ToString() const {
    return absl::Substitute("[format=$0 value=$1]", magic_enum::enum_name(format_code), Value());
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

  std::string ToString() const {
    return absl::Substitute("portal=$0 statement=$1 parameters=[$2] result_format_codes=[$3]",
                            dest_portal_name, src_prepared_stat_name,
                            absl::StrJoin(params, ", ", ToStringFormatter<Param>()),
                            absl::StrJoin(res_col_fmt_codes, ", ", FmtCodeFormatter()));
  }
};

struct ParamDesc {
  uint64_t timestamp_ns = 0;

  std::vector<int32_t> type_oids;
};

struct Parse {
  uint64_t timestamp_ns = 0;

  std::string_view stmt_name;
  std::string_view query;
  std::vector<int32_t> param_type_oids;

  std::string ToString() const { return std::string(query); }
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

    std::string ToString() const {
      // RowDesc::ToString() uses too much CPU time if absl::Substitute() is used, which costs
      // ~200ns CPU time in benchmark. This form using absl::StrCat() uses ~120ns.
      return absl::StrCat("name=", name, " table_oid=", table_oid, " attr_num=", attr_num,
                          " type_oid=", type_oid, " type_size=", type_size,
                          " type_modifier=", type_modifier,
                          " fmt_code=", magic_enum::enum_name(fmt_code));
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

  std::string ToString() const {
    return absl::StrCat(
        "ROW DESCRIPTION [",
        absl::StrJoin(fields,
                      // Avoid one concatenation in Field::ToString(), which saves additional
                      // 10ns. The version that appends "]" in Field::ToString() costs ~130ns,
                      // and this version takes ~120ns.
                      "] [", ToStringFormatter<Field>()),
        "]");
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

  std::string ToString() const {
    return absl::Substitute("type=$0 name=$1", magic_enum::enum_name(type), name);
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

    std::string ToString() const {
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
      out->append(err_field.ToString());
    }
  };

  std::string ToString() const {
    return absl::Substitute("ERROR RESPONSE [$0]", absl::StrJoin(fields, " ", ErrFieldFormatter()));
  }
};

struct DataRow {
  uint64_t timestamp_ns = 0;

  std::vector<std::optional<std::string_view>> cols;

  std::string ToString() const {
    return absl::StrJoin(cols, ",", [](std::string* out, const std::optional<std::string_view>& d) {
      out->append(d.has_value() ? d.value() : "[NULL]");
    });
  }
};

struct CmdCmpl {
  uint64_t timestamp_ns = 0;
  std::string_view cmd_tag;
};

struct ComboResp {
  uint64_t timestamp_ns = 0;

  std::variant<CmdCmpl, ErrResp> msg;

  std::string ToString() const {
    if (std::holds_alternative<CmdCmpl>(msg)) {
      return std::string(std::get<CmdCmpl>(msg).cmd_tag);
    }
    if (std::holds_alternative<ErrResp>(msg)) {
      return std::get<ErrResp>(msg).ToString();
    }
    CTX_DCHECK("Impossible!");
    return {};
  }
};

struct QueryReqResp {
  struct Query {
    uint64_t timestamp_ns = 0;

    std::string_view query;

    std::string ToString() const { return std::string(query); }
  };

  struct QueryResp {
    uint64_t timestamp_ns = 0;

    RowDesc row_desc;

    bool is_err_resp = false;

    std::vector<DataRow> data_rows;
    CmdCmpl cmd_cmpl;
    ErrResp err_resp;

    std::string ToString() const {
      if (is_err_resp) {
        return err_resp.ToString();
      }

      std::string res;

      if (!data_rows.empty()) {
        std::vector<std::string_view> field_names = row_desc.FieldNames();

        if (!field_names.empty()) {
          absl::StrAppend(&res, absl::StrJoin(field_names, ","));
          absl::StrAppend(&res, "\n");
        }

        absl::StrAppend(&res, absl::StrJoin(data_rows, "\n", ToStringFormatter<DataRow>()));
        absl::StrAppend(&res, "\n");
      }

      absl::StrAppend(&res, cmd_cmpl.cmd_tag);

      return res;
    }
  };

  Query req;
  QueryResp resp;
};

struct ParseReqResp {
  Parse req;
  ComboResp resp;
};

struct BindReqResp {
  BindRequest req;
  ComboResp resp;
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

    std::string ToString() const {
      if (is_err_resp) {
        return err_resp.ToString();
      }
      // It does not seem useful to format type OIDs, so we only report row description's field
      // names.
      return row_desc.ToString();
    }
  };

  Resp resp;
};

struct Exec {
  uint64_t timestamp_ns = 0;

  std::string_view query;
  std::vector<Param> params;

  std::string ToString() const {
    std::vector<std::string> param_strs;
    for (const auto& p : params) {
      param_strs.emplace_back(p.Value());
    }
    return absl::Substitute("query=[$0] params=[$1]", query, absl::StrJoin(param_strs, ", "));
  }
};

struct ExecReqResp {
  Exec req;
  QueryReqResp::QueryResp resp;
};

struct Record {
  RegularMessage req;
  RegularMessage resp;

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

struct State {
  absl::flat_hash_map<std::string, std::string> prepared_statements;

  // One postgres session can only have at most one unnamed statement.
  std::string unnamed_statement;

  // The last bound statement of the extended query session, without the parameters substituted. See
  // link for more info on extended query sessions:
  // https://www.postgresql.org/docs/10/protocol-flow.html#PROTOCOL-FLOW-EXT-QUERY
  std::string bound_statement;
  // The last set of parameters bound to bound_statement. Everytime a BIND command happens these are
  // invalidated.
  std::vector<Param> bound_params;
};

struct StateWrapper {
  State global;
  std::monostate send;
  std::monostate recv;
};

using connection_id_t = uint16_t;
struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = RegularMessage;
  using record_type = Record;
  using state_type = StateWrapper;
  using key_type = connection_id_t;
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
}  // namespace protocols
}  // namespace stirling
}  // namespace px
