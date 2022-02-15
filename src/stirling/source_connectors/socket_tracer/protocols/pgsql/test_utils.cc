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

#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/test_utils.h"

#include "src/common/base/byte_utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace pgsql {
namespace testutils {

namespace {
template <size_t N>
void AppendIntToByteString(std::string* output, int64_t val) {
  char int_str[N];
  utils::IntToBEndianBytes(val, int_str);
  output->append(std::string_view{int_str, N});
}
}  // namespace

std::string RegularMessageToByteString(const RegularMessage& msg) {
  std::string output;
  output.push_back(static_cast<char>(msg.tag));
  AppendIntToByteString<4>(&output, msg.payload.length() + 4);
  output.append(msg.payload);
  return output;
}

std::string CmdCmplToPayload(const CmdCmpl& cmd_cmpl) { return std::string(cmd_cmpl.cmd_tag); }

std::string CmdCmplToByteString(const CmdCmpl& cmd_cmpl) {
  RegularMessage msg;
  msg.tag = Tag::kCmdComplete;
  msg.payload = CmdCmplToPayload(cmd_cmpl);
  return RegularMessageToByteString(msg);
}

std::string DataRowToPayload(const DataRow& data_row) {
  std::string payload;
  AppendIntToByteString<2>(&payload, data_row.cols.size());
  for (const auto& col : data_row.cols) {
    if (!col.has_value()) {
      AppendIntToByteString<4>(&payload, -1);
      continue;
    }
    AppendIntToByteString<4>(&payload, col.value().size());
    payload.append(col.value());
  }
  return payload;
}

std::string DataRowToByteString(const DataRow& data_row) {
  RegularMessage msg;
  msg.tag = Tag::kDataRow;
  msg.payload = DataRowToPayload(data_row);
  return RegularMessageToByteString(msg);
}

std::string RowDescToPayload(const RowDesc& row_desc) {
  std::string payload;
  AppendIntToByteString<2>(&payload, row_desc.fields.size());
  for (const auto& field : row_desc.fields) {
    payload.append(field.name);
    payload.push_back('\0');
    AppendIntToByteString<4>(&payload, field.table_oid);
    AppendIntToByteString<2>(&payload, field.attr_num);
    AppendIntToByteString<4>(&payload, field.type_oid);
    AppendIntToByteString<2>(&payload, field.type_size);
    AppendIntToByteString<4>(&payload, field.type_modifier);
    AppendIntToByteString<2>(&payload, static_cast<int16_t>(field.fmt_code));
  }

  return payload;
}

std::string RowDescToByteString(const RowDesc& row_desc) {
  RegularMessage msg;
  msg.tag = Tag::kRowDesc;
  msg.payload = RowDescToPayload(row_desc);
  return RegularMessageToByteString(msg);
}

}  // namespace testutils
}  // namespace pgsql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
