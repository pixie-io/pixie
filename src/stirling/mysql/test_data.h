#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "src/common/base/base.h"
#include "src/stirling/mysql/mysql.h"

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
const StringRequest kStmtPrepareRequest(
    "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock JOIN sock_tag ON "
    "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=? GROUP "
    "BY id ORDER BY ?");

const StmtPrepareRespHeader kStmtPrepareRespHeader{2 /*stmt_id*/, 2 /*num_cols*/, 2 /*num_param*/,
                                                   0 /*num_warning*/};

// The following columns definitions and resultset rows are from real packet capture, but the
// contents don't really matter to the functionality of the test.
const std::vector<ColDefinition> kStmtPrepareParamDefs{
    ColDefinition{ConstString(
        "\x03\x64\x65\x66\x00\x00\x00\x01\x3f\x00\x0c\x3f\x00\x00\x00\x00\x00\xfd\x80\x00"
        "\x00\x00\x00")},
    ColDefinition{ConstString(
        "\x03\x64\x65\x66\x00\x00\x00\x01\x3f\x00\x0c\x3f\x00\x00\x00\x00\x00\xfd\x80\x00"
        "\x00\x00\x00")}};

const std::vector<ColDefinition> kStmtPrepareColDefs{
    ColDefinition{ConstString(
        "\x03\x64\x65\x66\x07\x73\x6f\x63\x6b\x73\x64\x62\x04\x73\x6f\x63\x6b\x04\x73\x6f"
        "\x63\x6b\x02"
        "\x69\x64\x07\x73\x6f\x63\x6b\x5f\x69\x64\x0c\x21\x00\x78\x00\x00\x00\xfd\x03\x50"
        "\x00\x00\x00")},
    ColDefinition{ConstString(
        "\x03\x64\x65\x66\x07\x73\x6f\x63\x6b\x73\x64\x62\x04\x73\x6f\x63\x6b\x04\x73\x6f\x63"
        "\x6b"
        "\x04"
        "\x6e\x61\x6d\x65\x04\x6e\x61\x6d\x65\x0c\x21\x00\x3c\x00\x00\x00\xfd\x00\x00\x00\x00"
        "\x00")}};

const StmtPrepareOKResponse kStmtPrepareResponse(kStmtPrepareRespHeader, kStmtPrepareColDefs,
                                                 kStmtPrepareParamDefs);

PreparedStatement kPreparedStatement{
    .request = std::string(kStmtPrepareRequest.msg()),
    .response = kStmtPrepareResponse,
};

/**
 * Statement Execute Event with 2 params, 2 col definitions, and 2 resultset rows.
 */
const std::vector<ParamPacket> kStmtExecuteParams = {
    {StmtExecuteParamType::kString, "\x62\x72\x6f\x77\x6e"},
    {StmtExecuteParamType::kString, "\x69\x64"}};

const StmtExecuteRequest kStmtExecuteRequest(2 /*stmt_id*/, kStmtExecuteParams /*params*/);

const std::vector<ColDefinition> kStmtExecuteColDefs = {
    ColDefinition{ConstString("\x03\x64\x65\x66\x07\x73\x6f\x63\x6b\x73\x64\x62"
                              "\x04\x73\x6f\x63\x6b\x04\x73\x6f\x63\x6b\x02\x69\x64\x07\x73\x6f"
                              "\x63\x6b\x5f\x69\x64\x0c\x21\x00\x78\x00\x00\x00\xfd\x01\x10\x00"
                              "\x00\x00")},
    ColDefinition{ConstString("\x03\x64\x65\x66\x07\x73\x6f\x63\x6b\x73\x64\x62\n"
                              "\x04\x73\x6f\x63\x6b\x04\x73\x6f\x63\x6b\x04\x6e\x61\x6d\x65\x04\n"
                              "\x6e\x61\x6d\x65\x0c\x21\x00\x3c\x00\x00\x00\xfd\x00\x00\x00\x00\n"
                              "\x00")}};

const std::vector<ResultsetRow> kStmtExecuteResultsetRows = {ResultsetRow{ConstString("\x03id1")},
                                                             ResultsetRow{ConstString("\x03id2")}};

const Resultset kStmtExecuteResultset(2, kStmtExecuteColDefs, kStmtExecuteResultsetRows);

/**
 * Statement Close Event
 */
const StmtCloseRequest kStmtCloseRequest(2 /*stmt_id*/);

/**
 * Query Event with 1 column and 3 resultset rows.
 */
const StringRequest kQueryRequest("SELECT name FROM tag;");

const std::vector<ColDefinition> kQueryColDefs = {
    ColDefinition{ConstString("\x2b\x00\x00\x02\x03\x64\x65\x66\x07\x73\x6f\x63\x6b\x73\x64\x62"
                              "\x03\x74\x61\x67\x03\x74\x61\x67\x04\x6e\x61\x6d\x65\x04\x6e\x61"
                              "\x6d\x65\x0c\x21\x00\x3c\x00\x00\x00\xfd\x00\x00\x00\x00\x00")}};

const std::vector<ResultsetRow> kQueryResultsetRows = {
    ResultsetRow{ConstString("\x05brown")},
    ResultsetRow{ConstString("\x04geek")},
    ResultsetRow{ConstString("\x06formal")},
};

const Resultset kQueryResultset(1, kQueryColDefs, kQueryResultsetRows);

}  // namespace testdata
}  // namespace mysql
}  // namespace stirling
}  // namespace pl
