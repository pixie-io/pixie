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
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
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
    ColDefinition{.catalog = "def",
                  .schema = "\x00",
                  .table = "\x00",
                  .org_table = "\x00",
                  .name = "?",
                  .org_name = "\x00",
                  .next_length = 12,
                  .character_set = 63,
                  .column_length = 0,
                  .column_type = ColType::kVarString,
                  .flags = 0x0080,
                  .decimals = 0x00},
    ColDefinition{.catalog = "def",
                  .schema = "\x00",
                  .table = "\x00",
                  .org_table = "\x00",
                  .name = "?",
                  .org_name = "\x00",
                  .next_length = 12,
                  .character_set = 63,
                  .column_length = 0,
                  .column_type = ColType::kVarString,
                  .flags = 0x0080,
                  .decimals = 0x00}};

const std::vector<ColDefinition> kStmtPrepareColDefs{
    ColDefinition{.catalog = "def",
                  .schema = "socksdb",
                  .table = "sock",
                  .org_table = "sock",
                  .name = "id",
                  .org_name = "sock_id",
                  .next_length = 12,
                  .character_set = 33,
                  .column_length = 120,
                  .column_type = ColType::kVarString,
                  .flags = 0x5003,
                  .decimals = 0x00},
    ColDefinition{.catalog = "def",
                  .schema = "socksdb",
                  .table = "sock",
                  .org_table = "sock",
                  .name = "name",
                  .org_name = "name",
                  .next_length = 12,
                  .character_set = 33,
                  .column_length = 60,
                  .column_type = ColType::kVarString,
                  .flags = 0x0000,
                  .decimals = 0x00}};

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
const std::vector<StmtExecuteParam> kStmtExecuteParams = {{ColType::kString, "brown"},
                                                          {ColType::kString, "id"}};

const StmtExecuteRequest kStmtExecuteRequest{.stmt_id = kStmtID, .params = kStmtExecuteParams};

const std::vector<ColDefinition> kStmtExecuteColDefs = {
    ColDefinition{.catalog = "def",
                  .schema = "socksdb",
                  .table = "sock",
                  .org_table = "sock",
                  .name = "id",
                  .org_name = "sock_id",
                  .next_length = 12,
                  .character_set = 33,
                  .column_length = 120,
                  .column_type = ColType::kVarString,
                  .flags = 0x1001,
                  .decimals = 0x00},
    ColDefinition{.catalog = "def",
                  .schema = "socksdb",
                  .table = "sock",
                  .org_table = "sock",
                  .name = "name",
                  .org_name = "name",
                  .next_length = 12,
                  .character_set = 33,
                  .column_length = 60,
                  .column_type = ColType::kVarString,
                  .flags = 0x0000,
                  .decimals = 0x00}};

// Binary Resultset Row
const std::vector<ResultsetRow> kStmtExecuteResultsetRows = {
    ResultsetRow{absl::StrCat(std::string(2, '\x00'), testutils::LengthEncodedString("id1"),
                              testutils::LengthEncodedString("name1"))},
    ResultsetRow{absl::StrCat(std::string(2, '\x00'), testutils::LengthEncodedString("id2"),
                              testutils::LengthEncodedString("name2"))}};

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

const std::vector<ColDefinition> kQueryColDefs = {ColDefinition{.catalog = "def",
                                                                .schema = "socksdb",
                                                                .table = "tag",
                                                                .org_table = "tag",
                                                                .name = "name",
                                                                .org_name = "name",
                                                                .next_length = 12,
                                                                .character_set = 33,
                                                                .column_length = 60,
                                                                .column_type = ColType::kVarString,
                                                                .flags = 0x0000,
                                                                .decimals = 0x00}};

// Text Resultset Row
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
  req.push_back(GenRawPacket(
      mysql::testutils::GenStringRequest(kStmtPrepareRequest, mysql::Command::kStmtPrepare)));
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
  req.push_back(
      GenRawPacket(mysql::testutils::GenStringRequest(kQueryRequest, mysql::Command::kQuery)));
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
}  // namespace protocols
}  // namespace stirling
}  // namespace px
