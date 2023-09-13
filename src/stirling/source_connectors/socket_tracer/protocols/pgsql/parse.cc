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

#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/parse.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include <absl/strings/ascii.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace pgsql {

// As a special case, -1 indicates a NULL column value. No value bytes follow in this case.
constexpr int kNullValLen = -1;

#define PX_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(expr, val_or) \
  PX_ASSIGN_OR(expr, val_or, return ParseState::kNeedsMoreData)

#define PX_ASSIGN_OR_RETURN_INVALID(expr, val_or) \
  PX_ASSIGN_OR(expr, val_or, return ParseState::kInvalid)

ParseState ParseRegularMessage(std::string_view* buf, RegularMessage* msg) {
  BinaryDecoder decoder(*buf);
  PX_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(const char char_val, decoder.ExtractChar());
  msg->tag = static_cast<Tag>(char_val);
  PX_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(msg->len, decoder.ExtractBEInt<int32_t>());

  constexpr int kLenFieldLen = 4;
  if (msg->len < kLenFieldLen) {
    // Len includes the len field itself, so its value cannot be less than the length of the field.
    return ParseState::kInvalid;
  }
  const size_t str_len = msg->len - 4;
  // Len includes the length field itself (int32_t), so the payload needs to exclude 4 bytes.
  PX_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(std::string_view str_view,
                                      decoder.ExtractString<char>(str_len));
  msg->payload = std::string(str_view);

  *buf = decoder.Buf();
  return ParseState::kSuccess;
}

Status ParseStartupMessage(std::string_view* buf, StartupMessage* msg) {
  BinaryDecoder decoder(*buf);

  PX_ASSIGN_OR_RETURN(msg->len, decoder.ExtractBEInt<int32_t>());
  PX_ASSIGN_OR_RETURN(msg->proto_ver.major, decoder.ExtractBEInt<int16_t>());
  PX_ASSIGN_OR_RETURN(msg->proto_ver.minor, decoder.ExtractBEInt<int16_t>());

  const size_t kHeaderSize = 2 * sizeof(int32_t);

  if (decoder.BufSize() < msg->len - kHeaderSize) {
    return error::InvalidArgument("Not enough data");
  }

  while (!decoder.eof()) {
    PX_ASSIGN_OR_RETURN(std::string_view name, decoder.ExtractStringUntil('\0'));
    if (name.empty()) {
      // Each name or value is terminated by '\0'. And all name value pairs are terminated by an
      // additional '\0'.
      //
      // Extracting an empty name means we are at the end of the string.
      break;
    }
    PX_ASSIGN_OR_RETURN(std::string_view value, decoder.ExtractStringUntil('\0'));
    if (value.empty()) {
      return error::InvalidArgument("Not enough data");
    }
    msg->nvs.push_back(NV{std::string(name), std::string(value)});
  }
  *buf = decoder.Buf();
  return Status::OK();
}

size_t FindFrameBoundary(std::string_view buf, size_t start) {
  for (size_t i = start; i < buf.size(); ++i) {
    if (magic_enum::enum_cast<Tag>(buf[i]).has_value()) {
      return i;
    }
  }
  return std::string_view::npos;
}

Status ParseCmdCmpl(const RegularMessage& msg, CmdCmpl* cmd_cmpl) {
  cmd_cmpl->timestamp_ns = msg.timestamp_ns;
  cmd_cmpl->cmd_tag = msg.payload;

  RemoveRepeatingSuffix(&cmd_cmpl->cmd_tag, '\0');

  return Status::OK();
}

// Given the input as the payload of a kRowDesc message, returns a list of column name.
// Row description format:
// | int16 field count |
// | Field description |
// ...
// Field description format:
// | string name | int32 table ID | int16 column number | int32 type ID | int16 type size |
// | int32 type modifier | int16 format code (text|binary) |
Status ParseRowDesc(const RegularMessage& msg, RowDesc* row_desc) {
  row_desc->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  PX_ASSIGN_OR_RETURN(const int16_t field_count, decoder.ExtractBEInt<int16_t>());

  for (int i = 0; i < field_count; ++i) {
    RowDesc::Field field = {};

    PX_ASSIGN_OR_RETURN(field.name, decoder.ExtractStringUntil('\0'));
    PX_ASSIGN_OR_RETURN(field.table_oid, decoder.ExtractBEInt<int32_t>());
    PX_ASSIGN_OR_RETURN(field.attr_num, decoder.ExtractBEInt<int16_t>());
    PX_ASSIGN_OR_RETURN(field.type_oid, decoder.ExtractBEInt<int32_t>());
    PX_ASSIGN_OR_RETURN(field.type_size, decoder.ExtractBEInt<int16_t>());
    PX_ASSIGN_OR_RETURN(field.type_modifier, decoder.ExtractBEInt<int32_t>());
    PX_ASSIGN_OR_RETURN(const int16_t fmt_code, decoder.ExtractBEInt<int16_t>());
    field.fmt_code = static_cast<FmtCode>(fmt_code);

    row_desc->fields.push_back(std::move(field));
  }
  CTX_DCHECK_EQ(decoder.BufSize(), 0U);
  return Status::OK();
}

Status ParseDataRow(const RegularMessage& msg, DataRow* data_row) {
  data_row->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  PX_ASSIGN_OR_RETURN(const int16_t field_count, decoder.ExtractBEInt<int16_t>());

  for (int i = 0; i < field_count; ++i) {
    // The length of the column value, in bytes (this count does not include itself). Can be zero.
    PX_ASSIGN_OR_RETURN(const auto value_len, decoder.ExtractBEInt<int32_t>());
    if (value_len == kNullValLen) {
      data_row->cols.push_back(std::nullopt);
      continue;
    }
    if (value_len == 0) {
      data_row->cols.push_back({});
      continue;
    }
    PX_ASSIGN_OR_RETURN(std::string_view value, decoder.ExtractString<char>(value_len));
    data_row->cols.push_back(value);
  }

  return Status::OK();
}

Status ParseBindRequest(const RegularMessage& msg, BindRequest* res) {
  res->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  // Any decoding error means the input is invalid. As the input must be the payload of a regular
  // message with tag 'B'. Therefore, it must meet a predefined pattern.
  PX_ASSIGN_OR_RETURN(res->dest_portal_name, decoder.ExtractStringUntil('\0'));
  PX_ASSIGN_OR_RETURN(res->src_prepared_stat_name, decoder.ExtractStringUntil('\0'));

  PX_ASSIGN_OR_RETURN(const int16_t param_fmt_code_count, decoder.ExtractBEInt<int16_t>());

  std::vector<FmtCode> param_fmt_codes;
  param_fmt_codes.reserve(param_fmt_code_count);

  for (int i = 0; i < param_fmt_code_count; ++i) {
    PX_ASSIGN_OR_RETURN(const int16_t fmt_code, decoder.ExtractBEInt<int16_t>());
    param_fmt_codes.push_back(static_cast<FmtCode>(fmt_code));
  }

  PX_ASSIGN_OR_RETURN(int16_t param_count, decoder.ExtractBEInt<int16_t>());

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
    CTX_DCHECK(i < param_fmt_codes.size());
    return param_fmt_codes[i];
  };

  // Check the >1 case: parameter format code count must match parameter count.
  if (param_fmt_codes.size() > 1 && param_fmt_codes.size() != static_cast<size_t>(param_count)) {
    return error::InvalidArgument(
        "Parameter format code count does not match parameter count, "
        "$0 vs. $1",
        param_fmt_codes.size(), param_count);
  }

  for (int i = 0; i < param_count; ++i) {
    PX_ASSIGN_OR_RETURN(int32_t param_value_len, decoder.ExtractBEInt<int32_t>());

    if (param_value_len == kNullValLen) {
      res->params.push_back({FmtCode::kText, std::nullopt});
      continue;
    }

    PX_ASSIGN_OR_RETURN(std::string_view param_value, decoder.ExtractString(param_value_len));
    const FmtCode code = get_format_code_for_ith_param(i);
    res->params.push_back({code, std::string(param_value)});
  }

  PX_ASSIGN_OR_RETURN(const int16_t res_col_fmt_code_count, decoder.ExtractBEInt<int16_t>());

  for (int i = 0; i < res_col_fmt_code_count; ++i) {
    PX_ASSIGN_OR_RETURN(const int16_t fmt_code, decoder.ExtractBEInt<int16_t>());
    res->res_col_fmt_codes.push_back(static_cast<FmtCode>(fmt_code));
  }

  return Status::OK();
}

Status ParseParamDesc(const RegularMessage& msg, ParamDesc* param_desc) {
  param_desc->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  PX_ASSIGN_OR_RETURN(const int16_t param_count, decoder.ExtractBEInt<int16_t>());

  for (int i = 0; i < param_count; ++i) {
    PX_ASSIGN_OR_RETURN(const int32_t type_oid, decoder.ExtractBEInt<int32_t>());
    param_desc->type_oids.push_back(type_oid);
  }

  return Status::OK();
}

Status ParseParse(const RegularMessage& msg, Parse* parse) {
  parse->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  PX_ASSIGN_OR_RETURN(parse->stmt_name, decoder.ExtractStringUntil<char>('\0'));

  PX_ASSIGN_OR_RETURN(parse->query, decoder.ExtractStringUntil<char>('\0'));

  PX_ASSIGN_OR_RETURN(const int16_t param_type_count, decoder.ExtractBEInt<int16_t>());
  for (int i = 0; i < param_type_count; ++i) {
    PX_ASSIGN_OR_RETURN(const int32_t type_oid, decoder.ExtractBEInt<int32_t>());
    parse->param_type_oids.push_back(type_oid);
  }

  return Status::OK();
}

Status ParseErrResp(const RegularMessage& msg, ErrResp* err_resp) {
  err_resp->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  while (!decoder.eof()) {
    PX_ASSIGN_OR_RETURN(const char code, decoder.ExtractChar());
    if (code == '\0') {
      // Reach end of stream.
      return decoder.BufSize() == 0
                 ? Status::OK()
                 : error::InvalidArgument("'\\x00' is not the last character of the payload");
    }
    PX_ASSIGN_OR_RETURN(std::string_view value, decoder.ExtractStringUntil('\0'));
    err_resp->fields.push_back({static_cast<ErrFieldCode>(code), value});
  }
  return Status::OK();
}

Status ParseDesc(const RegularMessage& msg, Desc* desc) {
  CTX_DCHECK_EQ(msg.tag, Tag::kDesc);

  desc->timestamp_ns = msg.timestamp_ns;

  BinaryDecoder decoder(msg.payload);

  PX_ASSIGN_OR_RETURN(const char type, decoder.ExtractChar());
  desc->type = static_cast<Desc::Type>(type);
  PX_ASSIGN_OR_RETURN(desc->name, decoder.ExtractStringUntil('\0'));

  return Status::OK();
}

}  // namespace pgsql

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, pgsql::RegularMessage* frame,
                      pgsql::StateWrapper* /*state*/) {
  PX_UNUSED(type);

  std::string_view buf_copy = *buf;
  pgsql::StartupMessage startup_msg = {};
  if (pgsql::ParseStartupMessage(&buf_copy, &startup_msg).ok() && !startup_msg.nvs.empty()) {
    // Ignore startup message, but remove it from the buffer.
    *buf = buf_copy;
  }
  return pgsql::ParseRegularMessage(buf, frame);
}

template <>
size_t FindFrameBoundary<pgsql::RegularMessage>(message_type_t type, std::string_view buf,
                                                size_t start, pgsql::StateWrapper* /*state*/) {
  PX_UNUSED(type);
  return pgsql::FindFrameBoundary(buf, start);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
