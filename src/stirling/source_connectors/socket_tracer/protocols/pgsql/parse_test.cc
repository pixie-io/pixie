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

#include <string>
#include <utility>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace pgsql {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

template <typename TagMatcher, typename LengthType, typename PayloadMatcher>
auto IsRegularMessage(TagMatcher tag, LengthType len, PayloadMatcher payload) {
  return AllOf(Field(&RegularMessage::tag, tag), Field(&RegularMessage::len, len),
               Field(&RegularMessage::payload, payload));
}

const std::string_view kQueryTestData =
    CreateStringView<char>("Q\000\000\000\033select * from account;\000");
const std::string_view kStartupMsgTestData = CreateStringView<char>(
    "\x00\x00\x00\x54\x00\003\x00\x00user\x00postgres\x00"
    "database\x00postgres\x00"
    "application_name\x00psql\x00"
    "client_encoding\x00UTF8\x00\x00");

TEST(PGSQLParseTest, BasicMessage) {
  std::string_view data = kQueryTestData;
  RegularMessage msg = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &msg));
  EXPECT_THAT(
      msg, IsRegularMessage(Tag::kQuery, 27, CreateStringView<char>("select * from account;\0")));
}

auto IsNV(std::string_view name, std::string_view value) {
  return AllOf(Field(&NV::name, name), Field(&NV::value, value));
}

TEST(PGSQLParseTest, StartupMessage) {
  std::string_view data = kStartupMsgTestData;
  StartupMessage msg = {};
  EXPECT_OK(ParseStartupMessage(&data, &msg));
  EXPECT_EQ(84, msg.len);
  EXPECT_EQ(3, msg.proto_ver.major);
  EXPECT_EQ(0, msg.proto_ver.minor);
  EXPECT_THAT(msg.nvs,
              ElementsAre(IsNV("user", "postgres"), IsNV("database", "postgres"),
                          IsNV("application_name", "psql"), IsNV("client_encoding", "UTF8")));
  EXPECT_THAT(data, IsEmpty());
}

const std::string_view kRowDescTestData = CreateStringView<char>(
    "T\000\000\000\246"
    "\000\006"
    "Name"
    "\000\000\000\004\356\000\002\000\000\000\023\000@\377\377\377\377\000\000"
    "Owner"
    "\000\000\000\000\000\000\000\000\000\000\023\000@\377\377\377\377\000\000"
    "Encoding"
    "\000\000\000\000\000\000\000\000\000\000\023\000@\377\377\377\377\000\000"
    "Collate"
    "\000\000\000\004\356\000\005\000\000\000\023\000@\377\377\377\377\000\000"
    "Ctype"
    "\000\000\000\004\356\000\006\000\000\000\023\000@\377\377\377\377\000\000"
    "Access "
    "privileges\000\000\000\000\000\000\000\000\000\000\031\377\377\377\377\377\377\000\000");

TEST(PGSQLParseTest, RowDesc) {
  RegularMessage msg;

  msg.timestamp_ns = 123;
  msg.payload = CreateStringView<char>(
      // Field count
      "\x00\x02"
      "first_name\x00"
      "\x00\x00\x40\x01\x00\x01\x00\x00\x00\x19\xff\xff\xff\xff"
      "\xff\xff\x00\x00"
      "last_name\x00"
      "\x00\x00\x40\x01\x00\x02\x00\x00\x00\x20\xff\xff\xff\xff"
      "\xff\xff\x00\x01");

  RowDesc row_desc;

  ASSERT_OK(ParseRowDesc(msg, &row_desc));

  EXPECT_EQ(123, row_desc.timestamp_ns);

  ASSERT_THAT(row_desc.fields, SizeIs(2));

  const auto& field1 = row_desc.fields.front();
  EXPECT_EQ(field1.name, "first_name");
  EXPECT_THAT(field1.table_oid, 16385);
  EXPECT_THAT(field1.attr_num, 1);
  EXPECT_THAT(field1.type_oid, 25);
  EXPECT_THAT(field1.type_size, -1);
  EXPECT_THAT(field1.type_modifier, -1);
  EXPECT_THAT(field1.fmt_code, FmtCode::kText);

  const auto& field2 = row_desc.fields[1];
  EXPECT_EQ(field2.name, "last_name");
  EXPECT_THAT(field2.table_oid, 16385);
  EXPECT_THAT(field2.attr_num, 2);
  EXPECT_THAT(field2.type_oid, 32);
  EXPECT_THAT(field2.type_size, -1);
  EXPECT_THAT(field2.type_modifier, -1);
  EXPECT_THAT(field2.fmt_code, FmtCode::kBinary);
}

TEST(PGSQLParseTest, RowDescGenerated) {
  RowDesc expected_row_desc;
  expected_row_desc.fields.push_back(RowDesc::Field{
      "first_name",
      16385,
      1,
      25,
      -1,
      -1,
      FmtCode::kText,
  });
  expected_row_desc.fields.push_back(RowDesc::Field{
      "last_name",
      16385,
      2,
      32,
      -1,
      -1,
      FmtCode::kBinary,
  });

  RegularMessage msg;
  msg.tag = Tag::kRowDesc;
  msg.timestamp_ns = 123;
  msg.payload = testutils::RowDescToPayload(expected_row_desc);

  RowDesc row_desc;
  ASSERT_OK(ParseRowDesc(msg, &row_desc));

  EXPECT_EQ(123, row_desc.timestamp_ns);

  ASSERT_THAT(row_desc.fields, SizeIs(2));

  const auto& field1 = row_desc.fields.front();
  EXPECT_EQ(field1.name, "first_name");
  EXPECT_THAT(field1.table_oid, 16385);
  EXPECT_THAT(field1.attr_num, 1);
  EXPECT_THAT(field1.type_oid, 25);
  EXPECT_THAT(field1.type_size, -1);
  EXPECT_THAT(field1.type_modifier, -1);
  EXPECT_THAT(field1.fmt_code, FmtCode::kText);

  const auto& field2 = row_desc.fields[1];
  EXPECT_EQ(field2.name, "last_name");
  EXPECT_THAT(field2.table_oid, 16385);
  EXPECT_THAT(field2.attr_num, 2);
  EXPECT_THAT(field2.type_oid, 32);
  EXPECT_THAT(field2.type_size, -1);
  EXPECT_THAT(field2.type_modifier, -1);
  EXPECT_THAT(field2.fmt_code, FmtCode::kBinary);
}

const std::string_view kDataRowTestData = CreateStringView<char>(
    "D"
    "\000\000\000F"
    "\000\006"
    "\000\000\000\010postgres"
    "\000\000\000\010postgres"
    "\000\000\000\004UTF8"
    "\000\000\000\nen_US.utf8"
    "\000\000\000\nen_US.utf8"
    "\377\377\377\377");

TEST(PGSQLParseTest, DataRow) {
  std::string_view data = kDataRowTestData;
  RegularMessage msg = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &msg));
  EXPECT_EQ(Tag::kDataRow, msg.tag);
  EXPECT_EQ(70, msg.len);

  DataRow data_row;
  ASSERT_OK(ParseDataRow(msg, &data_row));
  EXPECT_THAT(data_row.cols, ElementsAre("postgres", "postgres", "UTF8", "en_US.utf8", "en_US.utf8",
                                         std::nullopt));
}

TEST(PGSQLParseTest, DataRowGenerated) {
  DataRow expected_data_row;
  expected_data_row.cols.push_back("postgres");
  expected_data_row.cols.push_back("postgres");
  expected_data_row.cols.push_back("UTF8");
  expected_data_row.cols.push_back("en_US.utf8");
  expected_data_row.cols.push_back("en_US.utf8");
  expected_data_row.cols.push_back(std::nullopt);

  std::string data = testutils::DataRowToByteString(expected_data_row);
  std::string_view data_view(data);
  RegularMessage msg;
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data_view, &msg));
  EXPECT_EQ(Tag::kDataRow, msg.tag);
  EXPECT_EQ(70, msg.len);

  DataRow data_row;
  ASSERT_OK(ParseDataRow(msg, &data_row));
  EXPECT_THAT(data_row.cols, ElementsAre("postgres", "postgres", "UTF8", "en_US.utf8", "en_US.utf8",
                                         std::nullopt));
}

auto ParamIs(FmtCode fmt_code, std::string_view value) {
  return AllOf(Field(&Param::format_code, fmt_code), Field(&Param::value, value));
}

TEST(PGSQLParseTest, ParseBindRequest) {
  RegularMessage msg;
  msg.timestamp_ns = 123;
  msg.payload = CreateStringView<char>(
      // destination portal name
      "destination portal\x00"
      // source prepared statement name
      "source prepared statement\x00"
      // Parameter format code count
      "\x00\x01"
      // 1st parameter format code
      "\x00\x01"
      // Parameter count
      "\x00\x02"
      // 1st parameter value length
      "\x00\x00\x00\x05"
      // 1st parameter value
      "\x4a\x61\x73\x6f\x6e"
      // 2nd parameter value length
      "\x00\x00\x00\x03"
      // 2nd parameter value
      "\xaa\xbb\xcc"
      // Result column format code count
      "\x00\x02"
      // 1st result column format code
      "\x00\x00"
      // 2nd result column format code
      "\x00\x01");
  BindRequest bind_req;
  EXPECT_OK(ParseBindRequest(msg, &bind_req));
  EXPECT_EQ(123, bind_req.timestamp_ns);
  EXPECT_THAT(bind_req.dest_portal_name, StrEq("destination portal"));
  EXPECT_THAT(bind_req.src_prepared_stat_name, StrEq("source prepared statement"));
  EXPECT_THAT(bind_req.params, ElementsAre(ParamIs(FmtCode::kBinary, "\x4a\x61\x73\x6f\x6e"),
                                           ParamIs(FmtCode::kBinary, "\xaa\xbb\xcc")));
  EXPECT_THAT(bind_req.res_col_fmt_codes, ElementsAre(FmtCode::kText, FmtCode::kBinary));
}

TEST(PGSQLParseTest, ParseParamDesc) {
  RegularMessage msg;
  msg.timestamp_ns = 123;
  msg.payload = CreateStringView<char>(
      // Parameter count
      "\x00\x03"
      // 1st type oid
      "\x00\x00\x00\x19"
      // 2nd type oid
      "\x00\x00\x00\x19"
      // 2rd type oid
      "\x00\x00\x00\x19");

  ParamDesc param_desc;

  EXPECT_EQ(123, msg.timestamp_ns);
  EXPECT_OK(ParseParamDesc(msg, &param_desc));
  EXPECT_THAT(param_desc.type_oids, ElementsAre(25, 25, 25));
}

TEST(PGSQLParseTest, ParseParse) {
  RegularMessage msg;
  msg.timestamp_ns = 123;
  msg.payload = CreateStringView<char>(
      "test\x00"
      "SELECT * FROM person WHERE first_name=$1\x00"
      // Parameter type OIDs count
      "\x00\x01"
      "\x00\x00\x00\x19");
  Parse parse;
  EXPECT_OK(ParseParse(msg, &parse));
  EXPECT_EQ(123, msg.timestamp_ns);
  EXPECT_EQ(parse.stmt_name, "test");
  EXPECT_EQ(parse.query, "SELECT * FROM person WHERE first_name=$1");
  EXPECT_THAT(parse.param_type_oids, ElementsAre(25));
}

auto ErrFieldIs(ErrFieldCode code, std::string_view value) {
  return AllOf(Field(&ErrResp::Field::code, code), Field(&ErrResp::Field::value, value));
}

TEST(PGSQLParseTest, ParseErrResp) {
  RegularMessage msg;

  msg.timestamp_ns = 123;
  msg.payload = CreateStringView<char>(
      "\x53\x45\x52\x52\x4f\x52\x00\x56\x45\x52\x52"
      "\x4f\x52\x00\x43\x34\x32\x50\x30\x31\x00\x4d\x72\x65\x6c\x61\x74"
      "\x69\x6f\x6e\x20\x22\x78\x78\x78\x22\x20\x64\x6f\x65\x73\x20\x6e"
      "\x6f\x74\x20\x65\x78\x69\x73\x74\x00\x50\x31\x35\x00\x46\x70\x61"
      "\x72\x73\x65\x5f\x72\x65\x6c\x61\x74\x69\x6f\x6e\x2e\x63\x00\x4c"
      "\x31\x31\x39\x34\x00\x52\x70\x61\x72\x73\x65\x72\x4f\x70\x65\x6e"
      "\x54\x61\x62\x6c\x65\x00\x00");

  ErrResp err_resp;

  ASSERT_OK(ParseErrResp(msg, &err_resp));

  EXPECT_EQ(123, msg.timestamp_ns);
  EXPECT_THAT(err_resp.fields,
              ElementsAre(ErrFieldIs(ErrFieldCode::Severity, "ERROR"),
                          ErrFieldIs(ErrFieldCode::InternalSeverity, "ERROR"),
                          ErrFieldIs(ErrFieldCode::Code, "42P01"),
                          ErrFieldIs(ErrFieldCode::Message, "relation \"xxx\" does not exist"),
                          ErrFieldIs(ErrFieldCode::Position, "15"),
                          ErrFieldIs(ErrFieldCode::File, "parse_relation.c"),
                          ErrFieldIs(ErrFieldCode::Line, "1194"),
                          ErrFieldIs(ErrFieldCode::Routine, "parserOpenTable")));
}

TEST(PGSQLParseTest, ParseDesc) {
  RegularMessage msg;
  msg.timestamp_ns = 123;
  msg.tag = Tag::kDesc;
  msg.payload = CreateStringView<char>("Stest\0");

  Desc desc;
  ASSERT_OK(ParseDesc(msg, &desc));

  EXPECT_EQ(123, desc.timestamp_ns);
  EXPECT_EQ(Desc::Type::kStatement, desc.type);
  EXPECT_EQ("test", desc.name);
}

const std::string_view kDropTableCmplMsg =
    CreateStringView<char>("C\000\000\000\017DROP TABLE\000");

TEST(FindFrameBoundaryTest, FindTag) {
  EXPECT_EQ(0, FindFrameBoundary(kDropTableCmplMsg, 0));
  EXPECT_EQ(0, FindFrameBoundary(kRowDescTestData, 0));
  EXPECT_EQ(0, FindFrameBoundary(kDataRowTestData, 0));
  const std::string data = absl::StrCat("aaaaa", kDataRowTestData);
  EXPECT_EQ(5, FindFrameBoundary(data, 0));
}

TEST(PGSQLParseTest, ParseCmdCmplGenerated) {
  CmdCmpl expected_cmd_cmpl;
  expected_cmd_cmpl.cmd_tag = "UPDATE 10";
  auto data = testutils::CmdCmplToByteString(expected_cmd_cmpl);
  std::string_view data_view(data);
  RegularMessage msg = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data_view, &msg));

  CmdCmpl cmd_cmpl;
  EXPECT_OK(ParseCmdCmpl(msg, &cmd_cmpl));
  EXPECT_EQ(cmd_cmpl.cmd_tag, "UPDATE 10");
}

}  // namespace pgsql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
