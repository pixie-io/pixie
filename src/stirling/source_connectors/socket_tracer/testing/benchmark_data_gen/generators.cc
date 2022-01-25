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

#include "src/stirling/source_connectors/socket_tracer/testing/benchmark_data_gen/generators.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/frame_body_decoder.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_data.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"

namespace px {
namespace stirling {
namespace testing {

RecordGenerator::Record SingleReqRespGen::Next(int32_t) {
  RecordGenerator::Record record;
  record.frames.emplace_back(kIngress, req_bytes_);
  record.recv_bytes = req_bytes_.size();
  record.frames.emplace_back(kEgress, resp_bytes_);
  record.send_bytes = resp_bytes_.size();
  return record;
}

HTTP1SingleReqRespGen::HTTP1SingleReqRespGen(size_t total_size, size_t chunk_size, char c)
    : SingleReqRespGen(kDefaultHTTPReq, "") {
  size_t remaining = total_size;
  remaining -= req_bytes_.size();
  remaining -= (kDefaultHTTPRespFmt.size() - std::string_view("$0$1").size());

  std::string additional_headers;
  std::string body;
  if (chunk_size != 0) {
    additional_headers = "Transfer-Encoding: chunked\r\n";
    remaining -= additional_headers.size();
    std::string end_chunk = "0\r\n\r\n";
    remaining -= end_chunk.size();

    std::string chunk_hdr = absl::StrCat(absl::Hex(chunk_size), "\r\n");
    size_t chunk_overhead = chunk_hdr.size() + std::string_view("\r\n").size();
    size_t chunk_size_w_overhead = chunk_size + chunk_overhead;
    size_t num_chunks = remaining / chunk_size_w_overhead;
    for (size_t i = 0; i < num_chunks; ++i) {
      absl::StrAppend(&body, chunk_hdr, std::string(chunk_size, c), "\r\n");
    }
    remaining -= num_chunks * chunk_size_w_overhead;
    if (remaining > chunk_overhead) {
      remaining -= chunk_overhead;
      chunk_hdr = absl::StrCat(absl::Hex(remaining), "\r\n");
      absl::StrAppend(&body, chunk_hdr, std::string(remaining, c), "\r\n");
    }
    absl::StrAppend(&body, end_chunk);
  } else {
    std::string_view content_length_hdr_fmt = "Content-Length: $0\r\n";
    remaining -= content_length_hdr_fmt.size();
    // Estimate the length of the content string based on remaining before the subtraction.
    remaining -= (absl::StrCat(remaining).size() - std::string("$0").size());
    additional_headers = absl::Substitute(content_length_hdr_fmt, remaining);
    body = std::string(remaining, c);
  }

  resp_bytes_ = absl::Substitute(kDefaultHTTPRespFmt, additional_headers, body);
}

MySQLExecuteReqRespGen::MySQLExecuteReqRespGen(size_t total_size) : SingleReqRespGen("", "") {
  using protocols::mysql::ColDefinition;
  using protocols::mysql::ColType;
  using protocols::mysql::Resultset;
  using protocols::mysql::ResultsetRow;
  using protocols::mysql::StmtExecuteRequest;
  using protocols::mysql::testutils::GenRawPacket;
  using protocols::mysql::testutils::GenResultset;
  using protocols::mysql::testutils::GenResultsetRow;
  using protocols::mysql::testutils::GenStmtExecuteRequest;
  using protocols::mysql::testutils::LengthEncodedString;
  size_t remaining = total_size;

  StmtExecuteRequest req{
      1,
      {
          {ColType::kString, "col1"},
          {ColType::kString, "col2"},
      },
  };

  req_bytes_ = GenRawPacket(GenStmtExecuteRequest(req));
  remaining -= req_bytes_.size();

  Resultset result{
      .num_col = 2,
      .col_defs =
          std::vector<ColDefinition>{
              ColDefinition{
                  .catalog = "def",
                  .schema = "schema",
                  .table = "tbl",
                  .org_table = "tbl",
                  .name = "col1",
                  .org_name = "tbl_col1",
                  .next_length = 12,
                  .character_set = 33,
                  .column_length = 512,
                  .column_type = ColType::kVarString,
                  .flags = 0x1001,
                  .decimals = 0x00,
              },
              ColDefinition{
                  .catalog = "def",
                  .schema = "schema",
                  .table = "tbl",
                  .org_table = "tbl",
                  .name = "col2",
                  .org_name = "tbl_col2",
                  .next_length = 12,
                  .character_set = 33,
                  .column_length = 512,
                  .column_type = ColType::kVarString,
                  .flags = 0x1001,
                  .decimals = 0x00,
              },
          },
      .results = {},
  };

  for (auto packet : GenResultset(result)) {
    remaining -= GenRawPacket(packet).size();
  }

  ResultsetRow row;
  row.msg = absl::StrCat(std::string(2, '\x00'), LengthEncodedString(std::string(512, '1')),
                         LengthEncodedString(std::string(512, '2')));
  auto row_size = GenRawPacket(GenResultsetRow(0, row)).size();
  int num_rows = remaining / row_size;

  for (int i = 0; i < num_rows; ++i) {
    result.results.push_back(row);
  }

  for (auto packet : GenResultset(result)) {
    resp_bytes_ += GenRawPacket(packet);
  }
}

PostgresSelectReqRespGen::PostgresSelectReqRespGen(size_t total_size) : SingleReqRespGen("", "") {
  using protocols::pgsql::CmdCmpl;
  using protocols::pgsql::DataRow;
  using protocols::pgsql::RegularMessage;
  using protocols::pgsql::RowDesc;
  using protocols::pgsql::Tag;
  using protocols::pgsql::testutils::CmdCmplToByteString;
  using protocols::pgsql::testutils::DataRowToByteString;
  using protocols::pgsql::testutils::RegularMessageToByteString;
  using protocols::pgsql::testutils::RowDescToByteString;

  size_t remaining = total_size;

  RegularMessage req;
  req.tag = Tag::kQuery;
  req.payload = "select * from table\x00";
  req_bytes_ = RegularMessageToByteString(req);

  remaining -= req_bytes_.size();

  RowDesc row_desc;
  row_desc.fields.push_back(RowDesc::Field{
      .name = "col1",
      .table_oid = 0,
      .attr_num = 0,
      .type_oid = 1,
      .type_size = -1,
      .type_modifier = 0,
      .fmt_code = protocols::pgsql::FmtCode::kText,
  });
  row_desc.fields.push_back(RowDesc::Field{
      .name = "col2",
      .table_oid = 0,
      .attr_num = 0,
      .type_oid = 1,
      .type_size = -1,
      .type_modifier = 0,
      .fmt_code = protocols::pgsql::FmtCode::kText,
  });
  auto row_desc_bytes = RowDescToByteString(row_desc);
  remaining -= row_desc_bytes.size();

  RegularMessage ready_for_query;
  ready_for_query.tag = Tag::kReadyForQuery;
  ready_for_query.payload = "I";
  auto ready_for_query_bytes = RegularMessageToByteString(ready_for_query);
  remaining -= ready_for_query_bytes.size();

  DataRow row;
  row.cols.push_back(std::string(512, '1'));
  row.cols.push_back(std::string(512, '2'));
  auto row_bytes = DataRowToByteString(row);

  // Estimate CmdCmpl size based on the number of rows we would have without the CmdCmpl message.
  int num_rows_estimate = remaining / row_bytes.size();
  CmdCmpl cmd_cmpl;
  cmd_cmpl.cmd_tag = absl::StrCat("SELECT ", num_rows_estimate);
  remaining -= CmdCmplToByteString(cmd_cmpl).size();

  int num_rows = remaining / row_bytes.size();
  cmd_cmpl.cmd_tag = absl::StrCat("SELECT ", num_rows);
  auto cmd_cmpl_bytes = CmdCmplToByteString(cmd_cmpl);

  resp_bytes_ = row_desc_bytes;
  for (int i = 0; i < num_rows; ++i) {
    resp_bytes_ += row_bytes;
  }

  resp_bytes_ += cmd_cmpl_bytes;
  resp_bytes_ += ready_for_query_bytes;
}

CQLQueryReqRespGen::CQLQueryReqRespGen(size_t total_size)
    : SingleReqRespGen(/*req_bytes*/ "", /*resp_bytes*/ "") {
  using protocols::cass::ColSpec;
  using protocols::cass::QueryReq;
  using protocols::cass::ResultMetadata;
  using protocols::cass::ResultResp;
  using protocols::cass::ResultRespKind;
  using protocols::cass::ResultRowsResp;
  using protocols::cass::testutils::CreateCQLEvent;
  using protocols::cass::testutils::CreateCQLHeader;
  using protocols::cass::testutils::QueryReqToEvent;
  using protocols::cass::testutils::ResultRespToByteString;
  using protocols::cass::testutils::RowToByteString;

  size_t remaining = total_size;

  QueryReq req;
  req.query = "SELECT * FROM table";
  req.qp.consistency = 0;
  req.qp.flags = 0;

  uint16_t stream_id = 1;
  req_bytes_ = QueryReqToEvent(req, stream_id);
  remaining -= req_bytes_.size();

  // CQL has fixed header sizes, so we can pass in an empty body length to calculate its size.
  auto hdr_size = CreateCQLHeader(protocols::cass::RespOp::kResult, stream_id, 0).size();
  remaining -= hdr_size;

  ResultResp resp;
  resp.kind = ResultRespKind::kRows;

  ResultRowsResp rows_resp;
  rows_resp.metadata.flags = 0;
  rows_resp.metadata.columns_count = 2;
  ColSpec col_spec;
  col_spec.ks_name = "keyspace";
  col_spec.table_name = "table";
  col_spec.name = "col1";
  col_spec.type = {
      .type = protocols::cass::DataType::kVarchar,
      .value = "",
  };
  rows_resp.metadata.col_specs.push_back(col_spec);
  col_spec.name = "col2";
  rows_resp.metadata.col_specs.push_back(col_spec);

  // Estimate body size (without rows) by settings rows_count to 0.
  rows_resp.rows_count = 0;
  resp.resp = rows_resp;
  auto resp_body = ResultRespToByteString(resp);
  remaining -= resp_body.size();

  auto row = RowToByteString({"my col1 value", "my col2 value but longer"});
  auto rows_count = remaining / row.size();

  rows_resp.rows_count = rows_count;
  resp.resp = rows_resp;
  resp_body = ResultRespToByteString(resp);

  for (size_t i = 0; i < rows_count; ++i) {
    absl::StrAppend(&resp_body, row);
  }

  resp_bytes_ = CreateCQLEvent(protocols::cass::RespOp::kResult, resp_body, stream_id);
}

NATSMSGGen::NATSMSGGen(size_t total_size, char body_char) {
  const std::string_view msg_fmt("MSG MYSUBJECT 0 $0\r\n$1\r\n");
  size_t remaining = total_size;
  remaining -= msg_fmt.size() - std::string_view("$0$1").size();
  remaining -= absl::StrCat(remaining).size();
  msg_ = absl::Substitute(msg_fmt, remaining, std::string(remaining, body_char));
}

RecordGenerator::Record NATSMSGGen::Next(int32_t) {
  Record record;
  record.send_bytes = msg_.size();
  record.frames.push_back({kEgress, msg_});
  return record;
}

uint64_t NoGapsPosGenerator::NextPos(uint64_t msg_size) {
  uint64_t ret = pos_;
  pos_ += msg_size;
  return ret;
}

uint64_t GapPosGenerator::NextPos(uint64_t msg_size) {
  uint64_t ret = pos_;
  curr_segment_ += msg_size;
  if (curr_segment_ > max_segment_size_) {
    curr_segment_ = msg_size;
    pos_ += gap_size_;
    ret = pos_;
  }
  pos_ += msg_size;
  return ret;
}

uint64_t IterationGapPosGenerator::NextPos(uint64_t msg_size) {
  uint64_t ret = pos_;
  pos_ += msg_size;
  return ret;
}

void IterationGapPosGenerator::NextPollIteration() { pos_ += gap_size_; }

}  // namespace testing
}  // namespace stirling
}  // namespace px
