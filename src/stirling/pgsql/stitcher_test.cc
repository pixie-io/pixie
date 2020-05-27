#include "src/stirling/pgsql/stitcher.h"

#include "src/common/testing/testing.h"
#include "src/stirling/pgsql/parse.h"

namespace pl {
namespace stirling {
namespace pgsql {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;

template <typename PayloadMatcher>
auto HasPayloads(PayloadMatcher req_payload, PayloadMatcher resp_payload) {
  return AllOf(Field(&Record::req, Field(&RegularMessage::payload, req_payload)),
               Field(&Record::resp, Field(&RegularMessage::payload, resp_payload)));
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

TEST(PGSQLParseTest, AssembleQueryResp) {
  auto row_desc_data = kRowDescTestData;
  auto data_row_data = kDataRowTestData;

  RegularMessage m1 = {};
  RegularMessage m2 = {};
  RegularMessage m3 = {};
  m3.tag = Tag::kCmdComplete;
  m3.payload = "SELECT 1";

  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&row_desc_data, &m1));
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data_row_data, &m2));

  std::deque<RegularMessage> resps = {m1, m2, m3};

  auto begin = resps.begin();
  ASSERT_OK_AND_THAT(AssembleQueryResp(&begin, resps.end()),
                     Field(&RegularMessage::payload,
                           "Name,Owner,Encoding,Collate,Ctype,Access privileges\n"
                           "postgres,postgres,UTF8,en_US.utf8,en_US.utf8,[NULL]\n"
                           "SELECT 1"));
  EXPECT_EQ(begin, resps.end());
}

TEST(PGSQLParseTest, AssembleQueryRespFailures) {
  std::deque<RegularMessage> resps;
  auto begin = resps.begin();
  const auto end = resps.end();
  EXPECT_NOT_OK(AssembleQueryResp(&begin, end));
}

const std::string_view kReadyForQueryMsg = CreateStringView<char>("Z\000\000\000\005I");
const std::string_view kSyncMsg = CreateStringView<char>("S\000\000\000\004");

// TODO(yzhao): Consider creating RegularMessage objects directly rather than
// parsing from raw bytes.
StatusOr<std::deque<RegularMessage>> ParseRegularMessages(std::string_view data) {
  std::deque<RegularMessage> msgs;
  uint64_t ts = 100;
  RegularMessage msg;
  while (ParseRegularMessage(&data, &msg) == ParseState::kSuccess) {
    msg.timestamp_ns = ts++;
    msgs.push_back(std::move(msg));
  }
  if (!data.empty()) {
    return error::InvalidArgument("Some data are not parsed, left $0", data.size());
  }
  return msgs;
}

struct ProcessFramesTestCase {
  std::string_view req_data;
  std::string_view resp_data;
  std::vector<std::string_view> expected_req_msgs;
  std::vector<std::string_view> expected_resp_msgs;
};

class ProcessFramesTest : public ::testing::TestWithParam<ProcessFramesTestCase> {
 protected:
  State state_;
};

std::string_view GetQuery(const RegularMessage& msg) { return msg.payload; }

TEST_P(ProcessFramesTest, VerifySingleOutputMessage) {
  const ProcessFramesTestCase test_case = GetParam();

  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> reqs, ParseRegularMessages(test_case.req_data));
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> resps, ParseRegularMessages(test_case.resp_data));

  RecordsWithErrorCount<pgsql::Record> records_and_err_count =
      ProcessFrames(&reqs, &resps, &state_);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
  EXPECT_NE(0, records_and_err_count.records.front().req.payload.size());

  std::vector<uint64_t> req_tses;
  std::vector<std::string_view> req_msgs;
  for (const auto& record : records_and_err_count.records) {
    req_tses.push_back(record.req.timestamp_ns);
    req_msgs.push_back(record.req.payload);
  }

  std::vector<std::string_view> resp_msgs;
  for (const auto& record : records_and_err_count.records) {
    resp_msgs.push_back(record.resp.payload);
  }

  EXPECT_THAT(req_tses, ElementsAre(100, 100));

  EXPECT_THAT(req_msgs, ElementsAreArray(test_case.expected_req_msgs));
  EXPECT_THAT(resp_msgs, ElementsAreArray(test_case.expected_resp_msgs));

  EXPECT_EQ(0, records_and_err_count.error_count);
}

const std::string_view kParseData = CreateStringView<char>(
    // Parse
    "P"
    "\000\000\000\251"  // 169, payload is the next 165 bytes.
    "\000"
    "select t.oid, t.typname, t.typbasetype\n"
    "from pg_type t\n"
    "  join pg_type base_type on t.typbasetype=base_type.oid\n"
    "where t.typtype = \'d\'\n"
    "  and base_type.typtype = \'b\'\000\000\000"
    // kDesc
    "D\000\000\000\006S\000"
    // kSync
    "S\000\000\000\004"
    // kBind
    "B\000\000\000\020"
    // Portal
    "\000"
    // Statement
    "\000"
    // Number of parameter
    "\000\000"
    // Number of parameter format codes
    "\000\000"
    // Number of result column format codes
    "\000\002"
    // 2 kBinary
    "\000\001"
    "\000\001"
    // kExecute
    "E\000\000\000\011\000\000\000\000\000");

const std::string_view kParseRespData = CreateStringView<char>(
    // kParseComplete
    "1\000\000\000\004"
    // kParamDesc
    "t\000\000\000\006\000\000"
    // kRowDesc
    "T\000\000\000\124"
    // 3 fields
    "\000\003"
    "oid\000\000\000\004\337\377\376\000\000\000\032\000\004\377\377\377\377\000\000"
    "typname\000\000\000\004\337\000\001\000\000\000\023\000@\377\377\377\377\000\000"
    "typbasetype\000\000\000\004\337\000\030\000\000\000\032\000\004\377\377\377\377\000\000"
    // kReadyForQuery
    "Z\000\000\000\005I"
    // kBindComplete
    "2\000\000\000\004"
    // kDataRow
    "D\000\000\000\x29"
    // Number of columns
    "\000\003"
    // 4 bytes data
    "\000\000\000\004"
    "\000\0001\361"
    // 15 bytes data
    "\000\000\000\017"
    "cardinal_number"
    // 4 bytes data
    "\000\000\000\004"
    "\000\000\000\027"
    // kDataRow
    "D\000\000\000#\000\003\000\000\000\004\000\0001\375\000\000\000\tyes_or_"
    "no\000\000\000\004\000\000\004\023"
    // kDataRow
    "D\000\000\000(\000\003\000\000\000\004\000\0001\366\000\000\000\016sql_"
    "identifier\000\000\000\004\000\000\004\023"
    // kDataRow
    "D\000\000\000(\000\003\000\000\000\004\000\0001\364\000\000\000\016character_"
    "data\000\000\000\004\000\000\004\023"
    // kDataRow
    "D\000\000\000$\000\003\000\000\000\004\000\0001\373\000\000\000\ntime_"
    "stamp\000\000\000\004\000\000\004\240"
    // kCmdComplete
    "C\000\000\000\rSELECT 5\000");

const std::string_view kParseMsg =
    "select t.oid, t.typname, t.typbasetype\n"
    "from pg_type t\n"
    "  join pg_type base_type on t.typbasetype=base_type.oid\n"
    "where t.typtype = 'd'\n"
    "  and base_type.typtype = 'b'";

const std::string_view kParseRespMsg = CreateStringView<char>(
    "oid,typname,typbasetype\n"
    "\0\0\x31\xF1,cardinal_number,\0\0\0\x17\n"
    "\0\0\x31\xFD,yes_or_no,\0\0\x4\x13\n"
    "\0\0\x31\xF6,sql_identifier,\0\0\x4\x13\n"
    "\0\0\x31\xF4,character_data,\0\0\x4\x13\n"
    "\0\0\x31\xFB,time_stamp,\0\0\x4\xA0\nSELECT 5");
// Sent from Client.
const std::string_view kParseInsertData = CreateStringView<char>(
    "P\000\000\000\115"
    "\000INSERT INTO person (first_name, last_name, email) VALUES ($1, $2, $3)\000\000\000"
    // kDesc message, 'S' stands for a prepared statement, followed by an empty string as the name
    // of the prepared statement, which means the statement is unnamed.
    "D\000\000\000\006S\000"
    // kSync message.
    "S\000\000\000\004"
    // kBind.
    "B\000\000\000\066"
    "\000\000\000\000\000\003\000\000\000\005Jason\000\000\000\006Moiron\000\000\000\023jmoiron@"
    "jmoiron.net\000\000"
    // kExecute.
    "E\000\000\000\011"
    "\000\000\000\000\000");

const std::string_view kParseInsertRespData = CreateStringView<char>(
    // kParseComplete
    "1\000\000\000\004"
    // kParamDesc
    "t\000\000\000\022"
    "\000\003\000\000\000\031\000\000\000\031\000\000\000\031"
    // kNoData
    "n\000\000\000\004"
    // kReadyForQuery
    "Z\000\000\000\005T"
    // kBindComplete
    "2\000\000\000\004"
    // kCmdComplete
    "C\000\000\000\017INSERT 0 1\000");

const std::string_view kParseInsertMsg =
    "INSERT INTO person (first_name, last_name, email) VALUES ($1, $2, $3)";
const std::string_view kParseInsertRespMsg = "INSERT 0 1";

INSTANTIATE_TEST_SUITE_P(SingleOutputMessagePair, ProcessFramesTest,
                         ::testing::Values(ProcessFramesTestCase{kParseData,
                                                                 kParseRespData,
                                                                 {kParseMsg, kParseMsg},
                                                                 {"PARSE COMPLETE", kParseRespMsg}},
                                           ProcessFramesTestCase{
                                               kParseInsertData,
                                               kParseInsertRespData,
                                               {kParseInsertMsg, kParseInsertMsg},
                                               {"PARSE COMPLETE", kParseInsertRespMsg}}));

const std::string_view kCmdCompleteData = CreateStringView<char>("C\000\000\000\rSELECT 1\000");

// TODO(yzhao): Parameterize the following tests.
TEST_F(ProcessFramesTest, MatchQueryAndRowDesc) {
  constexpr char kQueryText[] = "select * from table;";
  RegularMessage q = {};
  q.tag = Tag::kQuery;
  q.len = sizeof(kQueryText) + sizeof(int32_t);
  q.payload = kQueryText;

  std::string_view data = kRowDescTestData;
  RegularMessage t = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &t));

  data = kDataRowTestData;
  RegularMessage d = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &d));

  data = kCmdCompleteData;
  RegularMessage c = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &c));

  std::deque<RegularMessage> reqs = {std::move(q)};
  std::deque<RegularMessage> resps = {std::move(t), std::move(d), std::move(c)};
  RecordsWithErrorCount<pgsql::Record> records_and_err_count =
      ProcessFrames(&reqs, &resps, &state_);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
  EXPECT_THAT(records_and_err_count.records,
              ElementsAre(HasPayloads("select * from table;",
                                      "Name,Owner,Encoding,Collate,Ctype,Access privileges\n"
                                      "postgres,postgres,UTF8,en_US.utf8,en_US.utf8,[NULL]\n"
                                      "SELECT 1")));
  EXPECT_EQ(0, records_and_err_count.error_count);
}

const std::string_view kDropTableCmplMsg =
    CreateStringView<char>("C\000\000\000\017DROP TABLE\000");

TEST_F(ProcessFramesTest, DropTable) {
  constexpr char kQueryText[] = "drop table foo;";
  RegularMessage q = {};
  q.tag = Tag::kQuery;
  q.len = sizeof(kQueryText) + sizeof(int32_t);
  q.payload = kQueryText;

  std::string_view data = kDropTableCmplMsg;
  RegularMessage c = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &c));

  std::deque<RegularMessage> reqs = {std::move(q)};
  std::deque<RegularMessage> resps = {std::move(c)};
  RecordsWithErrorCount<pgsql::Record> records_and_err_count =
      ProcessFrames(&reqs, &resps, &state_);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
  EXPECT_THAT(records_and_err_count.records,
              ElementsAre(HasPayloads("drop table foo;", "DROP TABLE")));
  EXPECT_EQ(0, records_and_err_count.error_count);
}

const std::string_view kRollbackMsg =
    CreateStringView<char>("\x51\x00\x00\x00\x0d\x52\x4f\x4c\x4c\x42\x41\x43\x4b\x00");
const std::string_view kRollbackCmplMsg =
    CreateStringView<char>("\x43\x00\x00\x00\x0d\x52\x4f\x4c\x4c\x42\x41\x43\x4b\x00");

TEST_F(ProcessFramesTest, Rollback) {
  RegularMessage rollback_msg = {};
  std::string_view data = kRollbackMsg;
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &rollback_msg));

  RegularMessage cmpl_msg = {};
  data = kRollbackCmplMsg;
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &cmpl_msg));

  std::deque<RegularMessage> reqs = {std::move(rollback_msg)};
  std::deque<RegularMessage> resps = {std::move(cmpl_msg)};
  RecordsWithErrorCount<pgsql::Record> records_and_err_count =
      ProcessFrames(&reqs, &resps, &state_);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
  EXPECT_THAT(records_and_err_count.records, ElementsAre(HasPayloads("ROLLBACK", "ROLLBACK")));
  EXPECT_EQ(0, records_and_err_count.error_count);
}

TEST_F(ProcessFramesTest, HandleParse) {
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> reqs, ParseRegularMessages(kParseData));
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> resps, ParseRegularMessages(kParseRespData));

  auto reqs_begin = reqs.begin();
  auto resps_begin = resps.begin();

  ParseReqResp req_resp;
  ASSERT_OK(HandleParse(*reqs_begin, &resps_begin, resps.end(), &req_resp, &state_));
  EXPECT_EQ(req_resp.req.query,
            "select t.oid, t.typname, t.typbasetype\nfrom pg_type t\n  join pg_type base_type on "
            "t.typbasetype=base_type.oid\nwhere t.typtype = 'd'\n  and base_type.typtype = 'b'");
  EXPECT_FALSE(req_resp.resp.has_value());
  EXPECT_EQ(resps_begin, resps.begin() + 1);
}

auto ErrFieldIs(ErrFieldCode code, std::string_view value) {
  return AllOf(Field(&ErrResp::Field::code, code), Field(&ErrResp::Field::value, value));
}

TEST_F(ProcessFramesTest, HandleParseErrResp) {
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> reqs, ParseRegularMessages(kParseData));

  std::string_view err_resp_data = CreateStringView<char>(
      "\x45\x00\x00\x00\x66\x53\x45\x52\x52\x4f\x52\x00\x56\x45\x52\x52"
      "\x4f\x52\x00\x43\x34\x32\x50\x30\x31\x00\x4d\x72\x65\x6c\x61\x74"
      "\x69\x6f\x6e\x20\x22\x78\x78\x78\x22\x20\x64\x6f\x65\x73\x20\x6e"
      "\x6f\x74\x20\x65\x78\x69\x73\x74\x00\x50\x31\x35\x00\x46\x70\x61"
      "\x72\x73\x65\x5f\x72\x65\x6c\x61\x74\x69\x6f\x6e\x2e\x63\x00\x4c"
      "\x31\x31\x39\x34\x00\x52\x70\x61\x72\x73\x65\x72\x4f\x70\x65\x6e"
      "\x54\x61\x62\x6c\x65\x00\x00");
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> resps, ParseRegularMessages(err_resp_data));

  auto reqs_begin = reqs.begin();
  auto resps_begin = resps.begin();

  ParseReqResp req_resp;
  ASSERT_OK(HandleParse(*reqs_begin, &resps_begin, resps.end(), &req_resp, &state_));
  EXPECT_EQ(req_resp.req.query,
            "select t.oid, t.typname, t.typbasetype\nfrom pg_type t\n  join pg_type base_type on "
            "t.typbasetype=base_type.oid\nwhere t.typtype = 'd'\n  and base_type.typtype = 'b'");
  ASSERT_TRUE(req_resp.resp.has_value());
  EXPECT_THAT(req_resp.resp.value().fields,
              ElementsAre(ErrFieldIs(ErrFieldCode::Severity, "ERROR"),
                          ErrFieldIs(ErrFieldCode::InternalSeverity, "ERROR"),
                          ErrFieldIs(ErrFieldCode::Code, "42P01"),
                          ErrFieldIs(ErrFieldCode::Message, "relation \"xxx\" does not exist"),
                          ErrFieldIs(ErrFieldCode::Position, "15"),
                          ErrFieldIs(ErrFieldCode::File, "parse_relation.c"),
                          ErrFieldIs(ErrFieldCode::Line, "1194"),
                          ErrFieldIs(ErrFieldCode::Routine, "parserOpenTable")));
  EXPECT_EQ(resps_begin, resps.begin() + 1);
}

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
