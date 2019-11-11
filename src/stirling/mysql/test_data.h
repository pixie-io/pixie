#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "src/common/base/base.h"
#include "src/stirling/mysql/mysql_types.h"
#include "src/stirling/mysql/test_utils.h"

namespace pl {
namespace stirling {
namespace mysql {
namespace testdata {

/**
 * A StmtPrepare and StmtExecute pair extracted from SockShop to test parsing and stitching
 * of MySQL packets and events. They are associated such that StmtExecute has the parameters
 * that can fit into the StmtPrepare's request.
 */

const int kStmtID = 2;

/**
 * Statement Prepare Event with 2 col definitions and 2 params.
 */
const StringRequest kStmtPrepareRequest{
    .msg =
        "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock JOIN sock_tag ON "
        "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=? "
        "GROUP "
        "BY id ORDER BY ?"};

const StmtPrepareRespHeader kStmtPrepareRespHeader{
    .stmt_id = kStmtID, .num_columns = 2, .num_params = 2, .warning_count = 0};

// The following columns definitions and resultset rows are from real packet capture, but the
// contents don't really matter to the functionality of the test.
const std::vector<ColDefinition> kStmtPrepareParamDefs{
    ColDefinition{
        testutils::LengthEncodedString("def") +
        ConstString(
            "\x00\x00\x00\x01\x3f\x00\x0c\x3f\x00\x00\x00\x00\x00\xfd\x80\x00\x00\x00\x00")},
    ColDefinition{
        testutils::LengthEncodedString("def") +
        ConstString(
            "\x00\x00\x00\x01\x3f\x00\x0c\x3f\x00\x00\x00\x00\x00\xfd\x80\x00\x00\x00\x00")}};

const std::vector<ColDefinition> kStmtPrepareColDefs{
    ColDefinition{testutils::LengthEncodedString("def") +
                  testutils::LengthEncodedString("socksdb") +
                  testutils::LengthEncodedString("sock") + testutils::LengthEncodedString("sock") +
                  testutils::LengthEncodedString("id") + testutils::LengthEncodedString("sock_id") +
                  ConstString("\x0c\x21\x00\x78\x00\x00\x00\xfd\x03\x50\x00\x00\x00")},
    ColDefinition{testutils::LengthEncodedString("def") +
                  testutils::LengthEncodedString("socksdb") +
                  testutils::LengthEncodedString("sock") + testutils::LengthEncodedString("sock") +
                  testutils::LengthEncodedString("name") + testutils::LengthEncodedString("name") +
                  ConstString("\x0c\x21\x00\x3c\x00\x00\x00\xfd\x00\x00\x00\x00\x00")}};

const StmtPrepareOKResponse kStmtPrepareResponse{.header = kStmtPrepareRespHeader,
                                                 .col_defs = kStmtPrepareColDefs,
                                                 .param_defs = kStmtPrepareParamDefs};

const PreparedStatement kPreparedStatement{
    .request = kStmtPrepareRequest.msg,
    .response = kStmtPrepareResponse,
};

/**
 * Statement Execute Event with 2 params, 2 col definitions, and 2 resultset rows.
 */
const std::vector<StmtExecuteParam> kStmtExecuteParams = {{MySQLColType::kString, "brown"},
                                                          {MySQLColType::kString, "id"}};

const StmtExecuteRequest kStmtExecuteRequest{.stmt_id = kStmtID, .params = kStmtExecuteParams};

const std::vector<ColDefinition> kStmtExecuteColDefs = {
    ColDefinition{testutils::LengthEncodedString("def") +
                  testutils::LengthEncodedString("socksdb") +
                  testutils::LengthEncodedString("sock") + testutils::LengthEncodedString("sock") +
                  testutils::LengthEncodedString("id") + testutils::LengthEncodedString("sock_id") +
                  ConstString("\x0c\x21\x00\x78\x00\x00\x00\xfd\x01\x10\x00\x00\x00")},
    ColDefinition{testutils::LengthEncodedString("def") +
                  testutils::LengthEncodedString("socksdb") +
                  testutils::LengthEncodedString("sock") + testutils::LengthEncodedString("sock") +
                  testutils::LengthEncodedString("name") + testutils::LengthEncodedString("name") +
                  ConstString("\x0c\x21\x00\x3c\x00\x00\x00\xfd\x00\x00\x00\x00\x00")}};

const std::vector<ResultsetRow> kStmtExecuteResultsetRows = {
    ResultsetRow{testutils::LengthEncodedString("id1")},
    ResultsetRow{testutils::LengthEncodedString("id2")}};

const Resultset kStmtExecuteResultset{
    .num_col = 2, .col_defs = kStmtExecuteColDefs, .results = kStmtExecuteResultsetRows};

/**
 * Statement Close Event
 */
const StmtCloseRequest kStmtCloseRequest{.stmt_id = kStmtID};

/**
 * Query Event with 1 column and 3 resultset rows.
 */
const StringRequest kQueryRequest{.msg = "SELECT name FROM tag;"};

const std::vector<ColDefinition> kQueryColDefs = {
    ColDefinition{ConstString("\x2b\x00\x00\x02") + testutils::LengthEncodedString("def") +
                  testutils::LengthEncodedString("socksdb") +
                  testutils::LengthEncodedString("tag") + testutils::LengthEncodedString("tag") +
                  testutils::LengthEncodedString("name") + testutils::LengthEncodedString("name") +
                  ConstString("\x0c\x21\x00\x3c\x00\x00\x00\xfd\x00\x00\x00\x00\x00")}};

const std::vector<ResultsetRow> kQueryResultsetRows = {
    ResultsetRow{testutils::LengthEncodedString("brown")},
    ResultsetRow{testutils::LengthEncodedString("geek")},
    ResultsetRow{testutils::LengthEncodedString("formal")},
};

const Resultset kQueryResultset{
    .num_col = 1, .col_defs = kQueryColDefs, .results = kQueryResultsetRows};

//----------------------------------------------------------------------------
// Raw Packet Data
//----------------------------------------------------------------------------

using mysql::testutils::GenRawPacket;

namespace impl {

std::vector<std::string> InitRawStmtPrepareReq() {
  std::vector<std::string> req;
  req.push_back(GenRawPacket(mysql::testutils::GenStringRequest(
      kStmtPrepareRequest, mysql::MySQLEventType::kStmtPrepare)));
  return req;
}

std::vector<std::string> InitRawStmtPrepareResp() {
  std::vector<std::string> resp;
  for (const auto& prepare_packet :
       mysql::testutils::GenStmtPrepareOKResponse(kStmtPrepareResponse)) {
    resp.push_back(GenRawPacket(prepare_packet));
  }
  return resp;
}

std::vector<std::string> InitRawStmtExecuteReq() {
  std::vector<std::string> req;
  req.push_back(
      mysql::testutils::GenRawPacket(mysql::testutils::GenStmtExecuteRequest(kStmtExecuteRequest)));
  return req;
}

std::vector<std::string> InitRawStmtExecuteResp() {
  std::vector<std::string> resp;
  for (const auto& execute_packet : mysql::testutils::GenResultset(kStmtExecuteResultset)) {
    resp.push_back(GenRawPacket(execute_packet));
  }
  return resp;
}

std::vector<std::string> InitRawStmtCloseReq() {
  std::vector<std::string> req;
  req.push_back(GenRawPacket(mysql::testutils::GenStmtCloseRequest(kStmtCloseRequest)));
  return req;
}

std::vector<std::string> InitRawQueryReq() {
  std::vector<std::string> req;
  req.push_back(GenRawPacket(
      mysql::testutils::GenStringRequest(kQueryRequest, mysql::MySQLEventType::kQuery)));
  return req;
}

std::vector<std::string> InitRawQueryResp() {
  std::vector<std::string> resp;
  for (const auto& execute_packet : mysql::testutils::GenResultset(kStmtExecuteResultset)) {
    resp.push_back(GenRawPacket(execute_packet));
  }
  return resp;
}

}  // namespace impl

const std::vector<std::string> kRawStmtPrepareReq = impl::InitRawStmtPrepareReq();
const std::vector<std::string> kRawStmtPrepareResp = impl::InitRawStmtPrepareResp();
const std::vector<std::string> kRawStmtExecuteReq = impl::InitRawStmtExecuteReq();
const std::vector<std::string> kRawStmtExecuteResp = impl::InitRawStmtExecuteResp();
const std::vector<std::string> kRawStmtCloseReq = impl::InitRawStmtCloseReq();
const std::vector<std::string> kRawQueryReq = impl::InitRawQueryReq();
const std::vector<std::string> kRawQueryResp = impl::InitRawQueryResp();

}  // namespace testdata
}  // namespace mysql
}  // namespace stirling
}  // namespace pl
