#include "src/stirling/pgsql/parse.h"

#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace pgsql {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

const std::string_view kQueryTestData =
    CreateStringView<char>("Q\000\000\000\033select * from account;\000");
const std::string_view kStartupMsgTestData = CreateStringView<char>(
    "\x00\x00\x00\x54\x00\003\x00\x00user\x00postgres\x00"
    "database\x00postgres\x00"
    "application_name\x00psql\x00"
    "client_encoding\x00UTF8\x00\x00");

auto IsRegularMessage(Tag tag, int32_t len, std::string_view payload) {
  return AllOf(Field(&RegularMessage::tag, tag), Field(&RegularMessage::len, len),
               Field(&RegularMessage::payload, payload));
}

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
  EXPECT_EQ(ParseState::kSuccess, ParseStartupMessage(&data, &msg));
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
  std::string_view data = kRowDescTestData;
  RegularMessage msg = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &msg));
  EXPECT_EQ(Tag::kRowDesc, msg.tag);
  EXPECT_EQ(166, msg.len);
  EXPECT_THAT(ParseRowDesc(msg.payload),
              ElementsAre("Name", "Owner", "Encoding", "Collate", "Ctype", "Access privileges"));
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
  EXPECT_THAT(ParseDataRow(msg.payload), ElementsAre("postgres", "postgres", "UTF8", "en_US.utf8",
                                                     "en_US.utf8", std::nullopt));
}

TEST(ProcessFramesTest, MatchQueryAndRowDesc) {
  constexpr char kQueryText[] = "select * from table;";
  RegularMessage q = {};
  q.tag = Tag::kQuery;
  q.len = sizeof(kQueryText) + sizeof(int32_t);
  q.payload = kQueryText;

  std::string_view data = kRowDescTestData;
  RegularMessage t = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &t));

  std::deque<RegularMessage> reqs = {std::move(q)};
  std::deque<RegularMessage> resps = {std::move(t)};
  RecordsWithErrorCount<pgsql::Record> records_and_err_count = ProcessFrames(&reqs, &resps);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
  EXPECT_THAT(
      records_and_err_count.records,
      ElementsAre(AllOf(
          Field(&Record::req, Field(&RegularMessage::payload, "select * from table;")),
          Field(&Record::resp, Field(&RegularMessage::payload,
                                     "Name,Owner,Encoding,Collate,Ctype,Access privileges")))));
  EXPECT_EQ(0, records_and_err_count.error_count);
}

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
