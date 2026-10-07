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

#include "src/vizier/services/agent/shared/vizier_results/result_conversion.h"

#include <optional>
#include <string>
#include <string_view>

namespace px {
namespace vizier {
namespace agent {

namespace {

void CopyRowBatch(const ::px::table_store::schemapb::RowBatchData& src, const std::string& table_id,
                  ::px::api::vizierpb::RowBatchData* dst) {
  dst->set_table_id(table_id);
  dst->set_num_rows(src.num_rows());
  dst->set_eow(src.eow());
  dst->set_eos(src.eos());

  for (const auto& col : src.cols()) {
    if (col.has_boolean_data()) {
      auto* out = dst->add_cols()->mutable_boolean_data();
      for (auto v : col.boolean_data().data()) {
        out->add_data(v);
      }
    } else if (col.has_int64_data()) {
      auto* out = dst->add_cols()->mutable_int64_data();
      for (auto v : col.int64_data().data()) {
        out->add_data(v);
      }
    } else if (col.has_time64ns_data()) {
      auto* out = dst->add_cols()->mutable_time64ns_data();
      for (auto v : col.time64ns_data().data()) {
        out->add_data(v);
      }
    } else if (col.has_float64_data()) {
      auto* out = dst->add_cols()->mutable_float64_data();
      for (auto v : col.float64_data().data()) {
        out->add_data(v);
      }
    } else if (col.has_string_data()) {
      auto* out = dst->add_cols()->mutable_string_data();
      for (const auto& v : col.string_data().data()) {
        out->add_data(v);
      }
    } else if (col.has_uint128_data()) {
      auto* out = dst->add_cols()->mutable_uint128_data();
      for (const auto& v : col.uint128_data().data()) {
        auto* n = out->add_data();
        n->set_low(v.low());
        n->set_high(v.high());
      }
    } else {
      // An unset column would silently shift every later column left, so keep
      // positions aligned by emitting an empty one.
      dst->add_cols();
    }
  }
}

void CopyExecutionStats(const ::px::carnotpb::TransferResultChunkRequest& chunk,
                        ::px::api::vizierpb::QueryExecutionStats* dst) {
  const auto& src = chunk.execution_and_timing_info().execution_stats();
  dst->set_bytes_processed(src.bytes_processed());
  dst->set_records_processed(src.records_processed());
  auto* timing = dst->mutable_timing();
  timing->set_execution_time_ns(src.timing().execution_time_ns());
  timing->set_compilation_time_ns(src.timing().compilation_time_ns());
}

}  // namespace

::px::api::vizierpb::DataType VizierDataTypeFromCarnot(::px::types::DataType t) {
  switch (t) {
    case ::px::types::BOOLEAN:
      return ::px::api::vizierpb::BOOLEAN;
    case ::px::types::INT64:
      return ::px::api::vizierpb::INT64;
    case ::px::types::UINT128:
      return ::px::api::vizierpb::UINT128;
    case ::px::types::FLOAT64:
      return ::px::api::vizierpb::FLOAT64;
    case ::px::types::STRING:
      return ::px::api::vizierpb::STRING;
    case ::px::types::TIME64NS:
      return ::px::api::vizierpb::TIME64NS;
    default:
      return ::px::api::vizierpb::DATA_TYPE_UNKNOWN;
  }
}

void EmitSchemaResponses(const ::px::carnot::planpb::Plan& plan, std::string_view query_id,
                         ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* writer) {
  for (const auto& fragment : plan.nodes()) {
    for (const auto& node : fragment.nodes()) {
      if (node.op().op_type() != ::px::carnot::planpb::OperatorType::GRPC_SINK_OPERATOR) {
        continue;
      }
      const auto& sink = node.op().grpc_sink_op();
      if (!sink.has_output_table()) {
        continue;
      }
      ::px::api::vizierpb::ExecuteScriptResponse resp;
      resp.set_query_id(std::string(query_id));
      auto* metadata = resp.mutable_meta_data();
      metadata->set_name(sink.output_table().table_name());
      metadata->set_id(sink.output_table().table_name());
      auto* relation = metadata->mutable_relation();
      for (int i = 0; i < sink.output_table().column_names().size(); ++i) {
        auto* col = relation->add_columns();
        col->set_column_name(sink.output_table().column_names()[i]);
        col->set_column_type(VizierDataTypeFromCarnot(
            static_cast<::px::types::DataType>(sink.output_table().column_types()[i])));
      }
      writer->Write(resp);
    }
  }
}

std::optional<::px::api::vizierpb::ExecuteScriptResponse> ExecuteScriptResponseFromChunk(
    const ::px::carnotpb::TransferResultChunkRequest& chunk, std::string_view query_id) {
  ::px::api::vizierpb::ExecuteScriptResponse resp;
  resp.set_query_id(std::string(query_id));
  bool has_payload = false;

  if (chunk.has_query_result() && chunk.query_result().has_row_batch()) {
    CopyRowBatch(chunk.query_result().row_batch(), chunk.query_result().table_name(),
                 resp.mutable_data()->mutable_batch());
    has_payload = true;
  }

  if (chunk.has_execution_and_timing_info() &&
      chunk.execution_and_timing_info().has_execution_stats()) {
    CopyExecutionStats(chunk, resp.mutable_data()->mutable_execution_stats());
    has_payload = true;
  }

  // A zero err_code is Carnot reporting success, not an error to surface.
  if (chunk.has_execution_error() && chunk.execution_error().err_code() != 0) {
    resp.mutable_status()->set_message(chunk.execution_error().msg());
    has_payload = true;
  }

  if (!has_payload) {
    return std::nullopt;
  }
  return resp;
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
