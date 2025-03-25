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

#include "src/stirling/source_connectors/socket_tracer/protocols/cql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/frame_body_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

// Equality operators for types. C++20 would make this a lot easier:
// https://en.cppreference.com/w/cpp/language/default_comparisons
bool operator==(const Option& a, const Option& b) { return a.type == b.type && a.value == b.value; }

bool operator==(const NameValuePair& a, const NameValuePair& b) {
  return a.name == b.name && a.value == b.value;
}

bool operator==(const QueryParameters& a, const QueryParameters& b) {
  return (a.consistency == b.consistency && a.flags == b.flags && a.values == b.values &&
          a.page_size == b.page_size && a.paging_state == b.paging_state &&
          a.serial_consistency == b.serial_consistency);
}

bool operator==(const ColSpec& a, const ColSpec& b) {
  return a.ks_name == b.ks_name && a.table_name == b.table_name && a.name == b.name &&
         a.type == b.type;
}

bool operator==(const ResultMetadata& a, const ResultMetadata& b) {
  return a.flags == b.flags && a.columns_count == b.columns_count &&
         a.paging_state == b.paging_state && a.gts_table_name == b.gts_table_name &&
         a.gts_keyspace_name == b.gts_keyspace_name && a.col_specs == b.col_specs;
}
bool operator==(const SchemaChange& a, const SchemaChange& b) {
  return a.change_type == b.change_type && a.target == b.target && a.keyspace == b.keyspace &&
         a.name == b.name && a.arg_types == b.arg_types;
}

bool operator==(const QueryReq& a, const QueryReq& b) { return a.query == b.query && a.qp == b.qp; }

bool operator==(const ResultVoidResp&, const ResultVoidResp&) { return true; }

bool operator==(const ResultRowsResp& a, const ResultRowsResp& b) {
  return a.metadata == b.metadata && a.rows_count == b.rows_count;
}
bool operator==(const ResultSetKeyspaceResp& a, const ResultSetKeyspaceResp& b) {
  return a.keyspace_name == b.keyspace_name;
}
bool operator==(const ResultPreparedResp& a, const ResultPreparedResp& b) {
  return a.id == b.id && a.metadata == b.metadata && a.result_metadata == b.result_metadata;
}

bool operator==(const ResultSchemaChangeResp& a, const ResultSchemaChangeResp& b) {
  return a.sc == b.sc;
}

bool operator==(const ResultResp& a, const ResultResp& b) {
  return a.kind == b.kind && a.resp == b.resp;
}

namespace testutils {

namespace {
template <size_t TIntSize>
void AppendIntToByteString(std::string* output, uint64_t val) {
  char int_str[TIntSize];
  utils::IntToBEndianBytes(val, int_str);
  output->append(std::string_view(int_str, TIntSize));
}

template <size_t TStrSizeBytes = 2, typename TStr = std::string>
void AppendStringToByteString(std::string* output, TStr str) {
  AppendIntToByteString<TStrSizeBytes>(output, str.size());
  output->append(std::string_view(reinterpret_cast<const char*>(str.data()), str.size()));
}

void AppendNameValuePairToByteString(std::string* output, const NameValuePair& name_val,
                                     bool use_names) {
  if (use_names) {
    AppendStringToByteString(output, name_val.name);
  }
  // the current NameValuePair struct doesn't account for the fact that `value` could be null
  // or "not set". For now we just output the value as if neither of those are possible.
  AppendStringToByteString(output, name_val.value);
}

void AppendQueryParametersToByteString(std::string* output, const QueryParameters& qp) {
  AppendIntToByteString<2>(output, qp.consistency);
  AppendIntToByteString<1>(output, qp.flags);

  // See https://github.com/apache/cassandra/blob/trunk/doc/native_protocol_v4.spec for flag
  // definitions.
  constexpr uint8_t kValuesFlag = 0x01;
  constexpr uint8_t kPageSizeFlag = 0x04;
  constexpr uint8_t kPagingStateFlag = 0x08;
  constexpr uint8_t kSerialConsistencyFlag = 0x10;
  constexpr uint8_t kTimestampFlag = 0x20;
  constexpr uint8_t kValuesNameFlag = 0x40;

  if (qp.flags & kValuesFlag) {
    AppendIntToByteString<2>(output, qp.values.size());
  }
  // Name value pair names are only output if a setting is set in qp.flags.
  bool use_names = qp.flags & kValuesNameFlag;
  for (const auto& name_val : qp.values) {
    AppendNameValuePairToByteString(output, name_val, use_names);
  }

  if (qp.flags & kPageSizeFlag) {
    AppendIntToByteString<4>(output, qp.page_size);
  }

  if (qp.flags & kPagingStateFlag) {
    // Current representation of `QueryParameters` doesn't allow for specifying `paging_state` as
    // NULL even though the spec does. For now we ignore the possiblity of NULL.
    AppendStringToByteString(output, qp.paging_state);
  }

  if (qp.flags & kSerialConsistencyFlag) {
    AppendIntToByteString<2>(output, qp.serial_consistency);
  }

  if (qp.flags & kTimestampFlag) {
    AppendIntToByteString<8>(output, qp.timestamp);
  }
}

void AppendColSpecToByteString(std::string* output, const ColSpec& col_spec,
                               bool global_tables_spec) {
  if (!global_tables_spec) {
    AppendStringToByteString(output, col_spec.ks_name);
    AppendStringToByteString(output, col_spec.table_name);
  }
  AppendStringToByteString(output, col_spec.name);
  AppendIntToByteString<2>(output, static_cast<uint16_t>(col_spec.type.type));
  if (col_spec.type.type == DataType::kCustom) {
    AppendStringToByteString(output, col_spec.type.value);
  }
}

void AppendResultMetadataToByteString(std::string* output, const ResultMetadata& metadata) {
  // See https://github.com/apache/cassandra/blob/trunk/doc/native_protocol_v4.spec for flag
  // definitions.
  constexpr uint32_t kGlobalTablesFlag = 0x0001;
  constexpr uint32_t kHasMorePagesFlag = 0x0002;
  constexpr uint32_t kNoMetadataFlag = 0x0004;

  AppendIntToByteString<4>(output, metadata.flags);
  AppendIntToByteString<4>(output, metadata.columns_count);

  if (metadata.flags & kHasMorePagesFlag) {
    AppendStringToByteString(output, metadata.paging_state);
  }

  if (metadata.flags & kNoMetadataFlag) {
    return;
  }

  bool global_tables_spec = metadata.flags & kGlobalTablesFlag;
  if (global_tables_spec) {
    AppendStringToByteString(output, metadata.gts_keyspace_name);
    AppendStringToByteString(output, metadata.gts_table_name);
  }
  for (const auto& col_spec : metadata.col_specs) {
    AppendColSpecToByteString(output, col_spec, global_tables_spec);
  }
}

void AppendRowsRespToBytesString(std::string* output, const ResultRowsResp& resp) {
  AppendResultMetadataToByteString(output, resp.metadata);
  AppendIntToByteString<4>(output, resp.rows_count);
}

void AppendSetKeyspaceRespToBytesString(std::string* output, const ResultSetKeyspaceResp& resp) {
  AppendStringToByteString(output, resp.keyspace_name);
}

void AppendPreparedRespToBytesString(std::string* output, const ResultPreparedResp& resp) {
  AppendStringToByteString(output, resp.id);

  AppendResultMetadataToByteString(output, resp.metadata);
  AppendResultMetadataToByteString(output, resp.result_metadata);
}

void AppendSchemaChangeToBytesString(std::string* output, const SchemaChange& sc) {
  AppendStringToByteString(output, sc.change_type);
  AppendStringToByteString(output, sc.target);
  AppendStringToByteString(output, sc.keyspace);

  if (sc.target != "KEYSPACE") {
    AppendStringToByteString(output, sc.name);
  }
  if (sc.target == "FUNCTION" || sc.target == "AGGREGATE") {
    AppendIntToByteString<2>(output, sc.arg_types.size());
    for (const auto& arg_type : sc.arg_types) {
      AppendStringToByteString(output, arg_type);
    }
  }
}

void AppendSchemaChangeRespToBytesString(std::string* output, const ResultSchemaChangeResp& resp) {
  AppendSchemaChangeToBytesString(output, resp.sc);
}

}  // namespace

std::string ResultRespToByteString(const ResultResp& result_resp) {
  std::string output;
  AppendIntToByteString<4>(&output, static_cast<uint8_t>(result_resp.kind));
  switch (result_resp.kind) {
    case ResultRespKind::kVoid:
      return output;
    case ResultRespKind::kRows: {
      AppendRowsRespToBytesString(&output, std::get<ResultRowsResp>(result_resp.resp));
      break;
    }
    case ResultRespKind::kSetKeyspace: {
      AppendSetKeyspaceRespToBytesString(&output,
                                         std::get<ResultSetKeyspaceResp>(result_resp.resp));
      break;
    }
    case ResultRespKind::kPrepared: {
      AppendPreparedRespToBytesString(&output, std::get<ResultPreparedResp>(result_resp.resp));
      break;
    }
    case ResultRespKind::kSchemaChange: {
      AppendSchemaChangeRespToBytesString(&output,
                                          std::get<ResultSchemaChangeResp>(result_resp.resp));
      break;
    }
  }
  return output;
}

std::string QueryReqToByteString(const QueryReq& req) {
  std::string output;
  AppendStringToByteString<4>(&output, req.query);
  AppendQueryParametersToByteString(&output, req.qp);
  return output;
}

std::string QueryReqToEvent(const QueryReq& req, uint16_t stream) {
  auto body = QueryReqToByteString(req);
  return CreateCQLEvent(ReqOp::kQuery, body, stream);
}

std::string RowToByteString(const std::vector<std::string_view>& cols) {
  std::string output;
  for (auto col : cols) {
    AppendIntToByteString<4>(&output, col.size());
    output.append(col);
  }
  return output;
}

}  // namespace testutils
}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
