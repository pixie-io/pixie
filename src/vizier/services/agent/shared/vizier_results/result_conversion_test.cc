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

#include <string>

#include "src/common/testing/testing.h"

namespace px {
namespace vizier {
namespace agent {

constexpr char kQueryID[] = "0000-test-query-id";

TEST(VizierDataTypeFromCarnot, MapsEveryKnownType) {
  EXPECT_EQ(::px::api::vizierpb::BOOLEAN, VizierDataTypeFromCarnot(::px::types::BOOLEAN));
  EXPECT_EQ(::px::api::vizierpb::INT64, VizierDataTypeFromCarnot(::px::types::INT64));
  EXPECT_EQ(::px::api::vizierpb::UINT128, VizierDataTypeFromCarnot(::px::types::UINT128));
  EXPECT_EQ(::px::api::vizierpb::FLOAT64, VizierDataTypeFromCarnot(::px::types::FLOAT64));
  EXPECT_EQ(::px::api::vizierpb::STRING, VizierDataTypeFromCarnot(::px::types::STRING));
  EXPECT_EQ(::px::api::vizierpb::TIME64NS, VizierDataTypeFromCarnot(::px::types::TIME64NS));
}

TEST(VizierDataTypeFromCarnot, UnknownTypeIsExplicit) {
  EXPECT_EQ(::px::api::vizierpb::DATA_TYPE_UNKNOWN,
            VizierDataTypeFromCarnot(::px::types::DATA_TYPE_UNKNOWN));
}

TEST(ExecuteScriptResponseFromChunk, InitiateConnIsSkipped) {
  // pxapi rejects an ExecuteScriptResponse with no meta_data, batch,
  // encrypted_batch or execution_stats as ErrInternalUnImplementedType, which
  // fails the entire stream. Handshake chunks must not be forwarded.
  ::px::carnotpb::TransferResultChunkRequest chunk;
  chunk.mutable_initiate_conn();
  EXPECT_FALSE(ExecuteScriptResponseFromChunk(chunk, kQueryID).has_value());
}

TEST(ExecuteScriptResponseFromChunk, EmptyChunkIsSkipped) {
  ::px::carnotpb::TransferResultChunkRequest chunk;
  EXPECT_FALSE(ExecuteScriptResponseFromChunk(chunk, kQueryID).has_value());
}

TEST(ExecuteScriptResponseFromChunk, ZeroCodeExecutionErrorIsSkipped) {
  // err_code 0 is Carnot reporting success, not an error to surface.
  ::px::carnotpb::TransferResultChunkRequest chunk;
  chunk.mutable_execution_error()->set_err_code(::px::statuspb::OK);
  EXPECT_FALSE(ExecuteScriptResponseFromChunk(chunk, kQueryID).has_value());
}

TEST(ExecuteScriptResponseFromChunk, NonZeroExecutionErrorIsSurfaced) {
  ::px::carnotpb::TransferResultChunkRequest chunk;
  auto* err = chunk.mutable_execution_error();
  err->set_err_code(::px::statuspb::INTERNAL);
  err->set_msg("boom");

  const auto resp = ExecuteScriptResponseFromChunk(chunk, kQueryID);
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(kQueryID, resp->query_id());
  EXPECT_EQ("boom", resp->status().message());
}

TEST(ExecuteScriptResponseFromChunk, CopiesRowBatchScalarFields) {
  ::px::carnotpb::TransferResultChunkRequest chunk;
  auto* result = chunk.mutable_query_result();
  result->set_table_name("output");
  auto* rb = result->mutable_row_batch();
  rb->set_num_rows(3);
  rb->set_eow(true);
  rb->set_eos(true);

  const auto resp = ExecuteScriptResponseFromChunk(chunk, kQueryID);
  ASSERT_TRUE(resp.has_value());
  const auto& batch = resp->data().batch();
  EXPECT_EQ("output", batch.table_id());
  EXPECT_EQ(3, batch.num_rows());
  EXPECT_TRUE(batch.eow());
  EXPECT_TRUE(batch.eos());
}

TEST(ExecuteScriptResponseFromChunk, CopiesEveryColumnType) {
  ::px::carnotpb::TransferResultChunkRequest chunk;
  auto* result = chunk.mutable_query_result();
  result->set_table_name("output");
  auto* rb = result->mutable_row_batch();
  rb->set_num_rows(2);

  rb->add_cols()->mutable_boolean_data()->add_data(true);
  rb->add_cols()->mutable_int64_data()->add_data(42);
  rb->add_cols()->mutable_time64ns_data()->add_data(1234567890);
  rb->add_cols()->mutable_float64_data()->add_data(2.5);
  rb->add_cols()->mutable_string_data()->add_data("hello");
  auto* u128 = rb->add_cols()->mutable_uint128_data()->add_data();
  u128->set_low(7);
  u128->set_high(9);

  const auto resp = ExecuteScriptResponseFromChunk(chunk, kQueryID);
  ASSERT_TRUE(resp.has_value());
  const auto& batch = resp->data().batch();
  ASSERT_EQ(6, batch.cols_size());
  EXPECT_TRUE(batch.cols(0).boolean_data().data(0));
  EXPECT_EQ(42, batch.cols(1).int64_data().data(0));
  EXPECT_EQ(1234567890, batch.cols(2).time64ns_data().data(0));
  EXPECT_DOUBLE_EQ(2.5, batch.cols(3).float64_data().data(0));
  EXPECT_EQ("hello", batch.cols(4).string_data().data(0));
  EXPECT_EQ(7U, batch.cols(5).uint128_data().data(0).low());
  EXPECT_EQ(9U, batch.cols(5).uint128_data().data(0).high());
}

TEST(ExecuteScriptResponseFromChunk, UnsetColumnKeepsPositionsAligned) {
  // Dropping an unrecognized column would shift every later column left, so a
  // client would silently read the wrong data under the wrong column name.
  ::px::carnotpb::TransferResultChunkRequest chunk;
  auto* rb = chunk.mutable_query_result()->mutable_row_batch();
  rb->add_cols();  // no oneof set
  rb->add_cols()->mutable_int64_data()->add_data(42);

  const auto resp = ExecuteScriptResponseFromChunk(chunk, kQueryID);
  ASSERT_TRUE(resp.has_value());
  ASSERT_EQ(2, resp->data().batch().cols_size());
  EXPECT_EQ(42, resp->data().batch().cols(1).int64_data().data(0));
}

TEST(ExecuteScriptResponseFromChunk, CopiesExecutionStats) {
  ::px::carnotpb::TransferResultChunkRequest chunk;
  auto* stats = chunk.mutable_execution_and_timing_info()->mutable_execution_stats();
  stats->set_bytes_processed(1024);
  stats->set_records_processed(16);
  stats->mutable_timing()->set_execution_time_ns(500);
  stats->mutable_timing()->set_compilation_time_ns(250);

  const auto resp = ExecuteScriptResponseFromChunk(chunk, kQueryID);
  ASSERT_TRUE(resp.has_value());
  const auto& out = resp->data().execution_stats();
  EXPECT_EQ(1024, out.bytes_processed());
  EXPECT_EQ(16, out.records_processed());
  EXPECT_EQ(500, out.timing().execution_time_ns());
  EXPECT_EQ(250, out.timing().compilation_time_ns());
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
