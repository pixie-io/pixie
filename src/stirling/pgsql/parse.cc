#include "src/stirling/pgsql/parse.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include <absl/strings/ascii.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/stirling/common/binary_decoder.h"

namespace pl {
namespace stirling {
namespace pgsql {

// As a special case, -1 indicates a NULL column value. No value bytes follow in this case.
constexpr int kNullValLen = -1;

#define PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(expr, val_or) \
  PL_ASSIGN_OR(expr, val_or, return ParseState::kNeedsMoreData)

#define PL_ASSIGN_OR_RETURN_INVALID(expr, val_or) \
  PL_ASSIGN_OR(expr, val_or, return ParseState::kInvalid)

ParseState ParseRegularMessage(std::string_view* buf, RegularMessage* msg) {
  BinaryDecoder decoder(*buf);
  PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(const char char_val, decoder.ExtractChar());
  msg->tag = static_cast<Tag>(char_val);
  PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(msg->len, decoder.ExtractInt<int32_t>());

  constexpr int kLenFieldLen = 4;
  if (msg->len < kLenFieldLen) {
    // Len includes the len field itself, so its value cannot be less than the length of the field.
    return ParseState::kInvalid;
  }
  const size_t str_len = msg->len - 4;
  // Len includes the length field itself (int32_t), so the payload needs to exclude 4 bytes.
  PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(std::string_view str_view,
                                      decoder.ExtractString<char>(str_len));
  msg->payload = std::string(str_view);

  *buf = decoder.Buf();
  return ParseState::kSuccess;
}

ParseState ParseStartupMessage(std::string_view* buf, StartupMessage* msg) {
  BinaryDecoder decoder(*buf);

  PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(msg->len, decoder.ExtractInt<int32_t>());
  PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(msg->proto_ver.major, decoder.ExtractInt<int16_t>());
  PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(msg->proto_ver.minor, decoder.ExtractInt<int16_t>());

  const size_t kHeaderSize = 2 * sizeof(int32_t);

  if (decoder.BufSize() < msg->len - kHeaderSize) {
    return ParseState::kNeedsMoreData;
  }

  while (!decoder.eof()) {
    PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(std::string_view name, decoder.ExtractStringUntil('\0'));
    if (name.empty()) {
      // Each name or value is terminated by '\0'. And all name value pairs are terminated by an
      // additional '\0'.
      //
      // Extracting an empty name means we are at the end of the string.
      break;
    }
    PL_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(std::string_view value, decoder.ExtractStringUntil('\0'));
    if (value.empty()) {
      return ParseState::kInvalid;
    }
    msg->nvs.push_back(NV{std::string(name), std::string(value)});
  }
  *buf = decoder.Buf();
  return ParseState::kSuccess;
}

size_t FindFrameBoundary(std::string_view buf, size_t start) {
  for (size_t i = start; i < buf.size(); ++i) {
    if (magic_enum::enum_cast<Tag>(buf[i]).has_value()) {
      return i;
    }
  }
  return std::string_view::npos;
}

#define PL_ASSIGN_OR_RETURN_RES(expr, val_or, res) PL_ASSIGN_OR(expr, val_or, return res)

std::vector<std::string_view> ParseRowDesc(std::string_view payload) {
  RowDesc row_desc;
  if (ParseRowDesc(payload, &row_desc) != ParseState::kSuccess) {
    return {};
  }

  std::vector<std::string_view> res;
  res.reserve(row_desc.fields.size());

  for (const auto& f : row_desc.fields) {
    res.push_back(f.name);
  }
  return res;
}

// Given the input as the payload of a kRowDesc message, returns a list of column name.
// Row description format:
// | int16 field count |
// | Field description |
// ...
// Field description format:
// | string name | int32 table ID | int16 column number | int32 type ID | int16 type size |
// | int32 type modifier | int16 format code (text|binary) |
ParseState ParseRowDesc(std::string_view payload, RowDesc* row_desc) {
  BinaryDecoder decoder(payload);

  PL_ASSIGN_OR_RETURN_INVALID(const int16_t field_count, decoder.ExtractInt<int16_t>());

  for (int i = 0; i < field_count; ++i) {
    RowDesc::Field field = {};

    PL_ASSIGN_OR_RETURN_INVALID(field.name, decoder.ExtractStringUntil('\0'));
    PL_ASSIGN_OR_RETURN_INVALID(field.table_oid, decoder.ExtractInt<int32_t>());
    PL_ASSIGN_OR_RETURN_INVALID(field.attr_num, decoder.ExtractInt<int16_t>());
    PL_ASSIGN_OR_RETURN_INVALID(field.type_oid, decoder.ExtractInt<int32_t>());
    PL_ASSIGN_OR_RETURN_INVALID(field.type_size, decoder.ExtractInt<int16_t>());
    PL_ASSIGN_OR_RETURN_INVALID(field.type_modifier, decoder.ExtractInt<int32_t>());
    PL_ASSIGN_OR_RETURN_INVALID(const int16_t fmt_code, decoder.ExtractInt<int16_t>());
    field.fmt_code = static_cast<FmtCode>(fmt_code);

    row_desc->fields.push_back(std::move(field));
  }
  DCHECK_EQ(decoder.BufSize(), 0);
  return ParseState::kSuccess;
}

std::vector<std::optional<std::string_view>> ParseDataRow(std::string_view data_row) {
  std::vector<std::optional<std::string_view>> res;

  BinaryDecoder decoder(data_row);
  PL_ASSIGN_OR_RETURN_RES(const int16_t field_count, decoder.ExtractInt<int16_t>(), res);
  for (int i = 0; i < field_count; ++i) {
    if (decoder.BufSize() < sizeof(int32_t)) {
      VLOG(1) << "Not enough data";
      return res;
    }
    // The length of the column value, in bytes (this count does not include itself). Can be zero.
    PL_ASSIGN_OR_RETURN_RES(int32_t value_len, decoder.ExtractInt<int32_t>(), res);
    if (value_len == kNullValLen) {
      res.push_back(std::nullopt);
      continue;
    }
    if (value_len == 0) {
      res.push_back({});
      continue;
    }
    if (decoder.BufSize() < static_cast<size_t>(value_len)) {
      VLOG(1) << "Not enough data, copy the rest of data";
      value_len = decoder.BufSize();
    }
    PL_ASSIGN_OR_RETURN_RES(std::string_view value, decoder.ExtractString<char>(value_len), res);
    res.push_back(value);
  }
  return res;
}

ParseState ParseBindRequest(const RegularMessage& msg, BindRequest* res) {
  res->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  // Any decoding error means the input is invalid. As the input must be the payload of a regular
  // message with tag 'B'. Therefore, it must meet a predefined pattern.
  PL_ASSIGN_OR_RETURN_INVALID(res->dest_portal_name, decoder.ExtractStringUntil('\0'));
  PL_ASSIGN_OR_RETURN_INVALID(res->src_prepared_stat_name, decoder.ExtractStringUntil('\0'));

  PL_ASSIGN_OR_RETURN_INVALID(const int16_t param_fmt_code_count, decoder.ExtractInt<int16_t>());

  std::vector<FmtCode> param_fmt_codes;
  param_fmt_codes.reserve(param_fmt_code_count);

  for (int i = 0; i < param_fmt_code_count; ++i) {
    PL_ASSIGN_OR_RETURN_INVALID(const int16_t fmt_code, decoder.ExtractInt<int16_t>());
    param_fmt_codes.push_back(static_cast<FmtCode>(fmt_code));
  }

  PL_ASSIGN_OR_RETURN_INVALID(int16_t param_count, decoder.ExtractInt<int16_t>());

  // The number of parameter format codes can be:
  // * 0: There is no parameter; or the format code is the default (TEXT) for all parameter.
  // * 1: There is at least 1 parameter, and the format code is the specified one for all parameter.
  // * >1: There is more than 1 parameter, and the format codes are exactly specified.
  auto get_format_code_for_ith_param = [&param_fmt_codes](size_t i) {
    if (param_fmt_codes.empty()) {
      return FmtCode::kText;
    }
    if (param_fmt_codes.size() == 1) {
      return param_fmt_codes.front();
    }
    DCHECK(i < param_fmt_codes.size());
    return param_fmt_codes[i];
  };

  // Check the >1 case: parameter format code count must match parameter count.
  if (param_fmt_codes.size() > 1 && param_fmt_codes.size() != static_cast<size_t>(param_count)) {
    return ParseState::kInvalid;
  }

  for (int i = 0; i < param_count; ++i) {
    PL_ASSIGN_OR_RETURN_INVALID(int16_t param_value_len, decoder.ExtractInt<int32_t>());

    if (param_value_len == kNullValLen) {
      res->params.push_back({FmtCode::kText, std::nullopt});
      continue;
    }

    PL_ASSIGN_OR_RETURN_INVALID(std::string_view param_value,
                                decoder.ExtractString(param_value_len));
    const FmtCode code = get_format_code_for_ith_param(i);
    res->params.push_back({code, std::string(param_value)});
  }

  PL_ASSIGN_OR_RETURN_INVALID(const int16_t res_col_fmt_code_count, decoder.ExtractInt<int16_t>());

  for (int i = 0; i < res_col_fmt_code_count; ++i) {
    PL_ASSIGN_OR_RETURN_INVALID(const int16_t fmt_code, decoder.ExtractInt<int16_t>());
    res->res_col_fmt_codes.push_back(static_cast<FmtCode>(fmt_code));
  }

  return ParseState::kSuccess;
}

ParseState ParseParamDesc(std::string_view payload, ParamDesc* param_desc) {
  BinaryDecoder decoder(payload);

  PL_ASSIGN_OR_RETURN_INVALID(const int16_t param_count, decoder.ExtractInt<int16_t>());

  for (int i = 0; i < param_count; ++i) {
    PL_ASSIGN_OR_RETURN_INVALID(const int32_t type_oid, decoder.ExtractInt<int32_t>());
    param_desc->type_oids.push_back(type_oid);
  }

  return ParseState::kSuccess;
}

Status ParseParse(const RegularMessage& msg, Parse* parse) {
  parse->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  PL_ASSIGN_OR_RETURN(parse->stmt_name, decoder.ExtractStringUntil<char>('\0'));

  PL_ASSIGN_OR_RETURN(parse->query, decoder.ExtractStringUntil<char>('\0'));

  PL_ASSIGN_OR_RETURN(const int16_t param_type_count, decoder.ExtractInt<int16_t>());
  for (int i = 0; i < param_type_count; ++i) {
    PL_ASSIGN_OR_RETURN(const int32_t type_oid, decoder.ExtractInt<int32_t>());
    parse->param_type_oids.push_back(type_oid);
  }

  return Status::OK();
}

Status ParseErrResp(std::string_view payload, ErrResp* err_resp) {
  BinaryDecoder decoder(payload);
  while (decoder.BufSize() != 0) {
    PL_ASSIGN_OR_RETURN(const char code, decoder.ExtractChar());
    if (code == '\0') {
      // Reach end of stream.
      return decoder.BufSize() == 0
                 ? Status::OK()
                 : error::InvalidArgument("'\\x00' is not the last character of the payload");
    }
    PL_ASSIGN_OR_RETURN(std::string_view value, decoder.ExtractStringUntil('\0'));
    err_resp->fields.push_back({static_cast<ErrFieldCode>(code), value});
  }
  return Status::OK();
}

Status ParseDesc(const RegularMessage& msg, Desc* desc) {
  DCHECK_EQ(msg.tag, Tag::kDesc);

  desc->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  PL_ASSIGN_OR_RETURN(const char type, decoder.ExtractChar());
  desc->type = static_cast<Desc::Type>(type);
  PL_ASSIGN_OR_RETURN(desc->name, decoder.ExtractStringUntil('\0'));

  return Status::OK();
}

}  // namespace pgsql

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, pgsql::RegularMessage* frame) {
  PL_UNUSED(type);

  std::string_view buf_copy = *buf;
  pgsql::StartupMessage startup_msg = {};
  if (ParseStartupMessage(&buf_copy, &startup_msg) == ParseState::kSuccess &&
      !startup_msg.nvs.empty()) {
    // Ignore startup message, but remove it from the buffer.
    *buf = buf_copy;
  }
  return ParseRegularMessage(buf, frame);
}

template <>
size_t FindFrameBoundary<pgsql::RegularMessage>(MessageType type, std::string_view buf,
                                                size_t start) {
  PL_UNUSED(type);
  return pgsql::FindFrameBoundary(buf, start);
}

}  // namespace stirling
}  // namespace pl
