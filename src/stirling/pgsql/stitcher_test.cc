#include "src/stirling/pgsql/stitcher.h"

#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/pgsql/parse.h"
#include "src/stirling/pgsql/test_data.h"

namespace pl {
namespace stirling {
namespace pgsql {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

template <typename PayloadMatcher>
auto HasPayloads(PayloadMatcher req_payload, PayloadMatcher resp_payload) {
  return AllOf(Field(&Record::req, Field(&RegularMessage::payload, req_payload)),
               Field(&Record::resp, Field(&RegularMessage::payload, resp_payload)));
}

TEST(PGSQLParseTest, FillQueryResp) {
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

  QueryReqResp::QueryResp query_resp;
  auto begin = resps.begin();
  ASSERT_OK(FillQueryResp(&begin, resps.end(), &query_resp));
  EXPECT_EQ(
      "Name,Owner,Encoding,Collate,Ctype,Access privileges\n"
      "postgres,postgres,UTF8,en_US.utf8,en_US.utf8,[NULL]\n"
      "SELECT 1",
      query_resp.ToString());
  EXPECT_EQ(begin, resps.end());
}

TEST(PGSQLParseTest, FillQueryRespFailures) {
  std::deque<RegularMessage> resps;
  auto begin = resps.begin();
  const auto end = resps.end();

  QueryReqResp::QueryResp query_resp;

  EXPECT_NOT_OK(FillQueryResp(&begin, end, &query_resp));
}

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

class ProcessFramesTest : public ::testing::Test {
 protected:
  State state_;
};

TEST_F(ProcessFramesTest, VerifySingleOutputMessage) {
  ASSERT_OK_AND_ASSIGN(
      std::deque<RegularMessage> reqs,
      ParseRegularMessages(absl::StrCat(kParseData1, kDescData, kBindData, kExecData)));
  ASSERT_OK_AND_ASSIGN(
      std::deque<RegularMessage> resps,
      ParseRegularMessages(absl::StrCat(kParseCmplData, kParamDescData, kRowDescData, kBindCmplData,
                                        kDataRowData, kCmdCmplData)));

  for (auto& resp : resps) {
    // Increment response timestamp_ns, so they all start after requests, which makes the test more
    // easier to reason with.
    resp.timestamp_ns += 10;
  }

  RecordsWithErrorCount<pgsql::Record> records_and_err_count =
      ProcessFrames(&reqs, &resps, &state_);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
  EXPECT_NE(0, records_and_err_count.records.front().req.payload.size());

  std::vector<uint64_t> req_tses;
  std::vector<std::string> req_msgs;
  for (const auto& record : records_and_err_count.records) {
    req_tses.push_back(record.req.timestamp_ns);
    req_msgs.push_back(record.req.payload);
  }

  std::vector<uint64_t> resp_tses;
  std::vector<std::string> resp_msgs;
  for (const auto& record : records_and_err_count.records) {
    resp_tses.push_back(record.resp.timestamp_ns);
    resp_msgs.push_back(record.resp.payload);
  }

  EXPECT_THAT(
      req_msgs,
      ElementsAre(
          "SELECT * FROM person WHERE first_name=$1", "DESCRIBE [type=kStatement name=]",
          "BIND [portal= statement= parameters=[[formt=kText value=Jason]] result_format_codes=[]]",
          "EXECUTE [SELECT * FROM person WHERE first_name=Jason]"));
  EXPECT_THAT(resp_msgs,
              ElementsAre("PARSE COMPLETE",
                          "ROW DESCRIPTION "
                          "[name=first_name table_oid=16384 attr_num=1 type_oid=25 type_size=-1 "
                          "type_modifier=-1 fmt_code=kText] "
                          "[name=last_name table_oid=16384 attr_num=2 type_oid=25 type_size=-1 "
                          "type_modifier=-1 fmt_code=kText] "
                          "[name=email table_oid=16384 attr_num=3 type_oid=25 type_size=-1 "
                          "type_modifier=-1 fmt_code=kText]",
                          "BIND COMPLETE", "Jason,Moiron,jmoiron@jmoiron.net\nSELECT 238"));

  EXPECT_EQ(0, records_and_err_count.error_count);
}

// TODO(yzhao): Parameterize the following tests.
TEST_F(ProcessFramesTest, SimpleQuery) {
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

  data = kCmdCmplData;
  RegularMessage c = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &c));

  std::deque<RegularMessage> reqs = {std::move(q)};
  std::deque<RegularMessage> resps = {std::move(t), std::move(d), std::move(c)};
  RecordsWithErrorCount<pgsql::Record> records_and_err_count =
      ProcessFrames(&reqs, &resps, &state_);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
  EXPECT_THAT(records_and_err_count.records,
              ElementsAre(HasPayloads("QUERY [select * from table;]",
                                      "Name,Owner,Encoding,Collate,Ctype,Access privileges\n"
                                      "postgres,postgres,UTF8,en_US.utf8,en_US.utf8,[NULL]\n"
                                      "SELECT 238")));
  EXPECT_EQ(0, records_and_err_count.error_count);
}

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
              ElementsAre(HasPayloads("QUERY [drop table foo;]", "DROP TABLE")));
  EXPECT_EQ(0, records_and_err_count.error_count);
}

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
  EXPECT_THAT(records_and_err_count.records,
              ElementsAre(HasPayloads("QUERY [ROLLBACK]", "ROLLBACK")));
  EXPECT_EQ(0, records_and_err_count.error_count);
}

TEST_F(ProcessFramesTest, HandleParseErrResp) {
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> reqs, ParseRegularMessages(kParseData1));
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> resps, ParseRegularMessages(kErrRespData));

  auto reqs_begin = reqs.begin();
  auto resps_begin = resps.begin();

  ParseReqResp req_resp;
  ASSERT_OK(HandleParse(*reqs_begin, &resps_begin, resps.end(), &req_resp, &state_));
  EXPECT_EQ("SELECT * FROM person WHERE first_name=$1", req_resp.req.query);
  EXPECT_EQ(
      "ERROR RESPONSE [Severity=ERROR InternalSeverity=ERROR Code=42P01 "
      "Message=relation \"xxx\" does not exist Position=15 "
      "File=parse_relation.c Line=1194 Routine=parserOpenTable]",
      req_resp.resp.ToString());
  EXPECT_EQ(resps_begin, resps.begin() + 1);
}

struct HandleBindTestCase {
  std::string_view req_data;
  std::string_view resp_data;
  std::string_view expected_bound_statement;
};

std::ostream& operator<<(std::ostream& os, const HandleBindTestCase& test_case) {
  os << absl::Substitute("[expected_bound_statement='$0']", test_case.expected_bound_statement);
  return os;
}

class HandleBindTest : public ::testing::TestWithParam<HandleBindTestCase> {
 protected:
  void SetUp() override {
    constexpr char kStmt[] = "select $1, $2 from t";
    state_.unnamed_statement = kStmt;
    state_.prepared_statements["foo"] = kStmt;
  }

  State state_;
};

TEST_P(HandleBindTest, CheckBoundStatement) {
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> reqs, ParseRegularMessages(GetParam().req_data));
  ASSERT_OK_AND_ASSIGN(std::deque<RegularMessage> resps,
                       ParseRegularMessages(GetParam().resp_data));

  auto resp_iter = resps.begin();
  BindReqResp req_resp;

  EXPECT_OK(HandleBind(reqs.front(), &resp_iter, resps.end(), &req_resp, &state_));

  EXPECT_EQ(resp_iter, resps.end());

  EXPECT_EQ(100, req_resp.req.timestamp_ns);
  EXPECT_THAT(req_resp.req.params, SizeIs(2));

  EXPECT_EQ("BIND COMPLETE", req_resp.resp.ToString());

  EXPECT_EQ(GetParam().expected_bound_statement, state_.bound_statement);
}

INSTANTIATE_TEST_SUITE_P(
    UnnamedAndNamedStatements, HandleBindTest,
    ::testing::Values(HandleBindTestCase{kBindUnnamedData, kBindCmplData, "select foo, bar from t"},
                      HandleBindTestCase{kBindNamedData, kBindCmplData, "select foo, bar from t"}));

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
