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

#include <filesystem>

#include "src/common/testing/testing.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/demo_apps/go_http/go_http_fixture.h"

namespace px {
namespace stirling {

namespace http = protocols::http;

using ::px::stirling::testing::SocketTraceBPFTestFixture;

using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

class GoHTTPTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
  GoHTTPTraceTest() : SocketTraceBPFTestFixture() {}

  void SetUp() override {
    SocketTraceBPFTestFixture::SetUp();
    go_http_fixture_.LaunchServer();
  }

  void TearDown() override {
    SocketTraceBPFTestFixture::TearDown();
    go_http_fixture_.ShutDown();
  }

  testing::GoHTTPFixture go_http_fixture_;

  DataTable data_table_{/*id*/ 0, kHTTPTable};
};

TEST_F(GoHTTPTraceTest, RequestAndResponse) {
  StartTransferDataThread();

  // Uncomment to enable tracing:
  // FLAGS_stirling_conn_trace_pid = go_http_fixture_.server_pid();

  go_http_fixture_.LaunchGetClient();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  // By default, we do not trace the client.
  EXPECT_THAT(
      testing::FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, go_http_fixture_.client_pid()),
      IsEmpty());

  // We do expect to trace the server.
  const std::vector<size_t> target_record_indices =
      testing::FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, go_http_fixture_.server_pid());
  ASSERT_THAT(target_record_indices, SizeIs(1));

  const size_t target_record_idx = target_record_indices.front();

  EXPECT_THAT(
      std::string(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      AllOf(HasSubstr(R"("Accept-Encoding":"gzip")"),
            HasSubstr(absl::Substitute(R"(Host":"localhost:$0")", go_http_fixture_.server_port())),
            ContainsRegex(R"(User-Agent":"Go-http-client/.+")")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      AllOf(HasSubstr(R"("Content-Length":"31")"), HasSubstr(R"(Content-Type":"json)")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(target_record_idx)),
      // On IPv6 host, localhost is resolved to ::1.
      AnyOf(HasSubstr("127.0.0.1"), HasSubstr("::1")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPLocalAddrIdx]->Get<types::StringValue>(target_record_idx)),
      // Due to loopback, the local address is the same as the remote address.
      AnyOf(HasSubstr("127.0.0.1"), HasSubstr("::1")));
  EXPECT_THAT(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(target_record_idx),
              StrEq(absl::StrCat(R"({"greeter":"Hello PixieLabs!"})", "\n")));
  // This test currently performs client-side tracing because of the cluster CIDR in
  // socket_trace_bpf_test_fixture.h.
  EXPECT_EQ(record_batch[kHTTPTraceRoleIdx]->Get<types::Int64Value>(target_record_idx).val,
            static_cast<int>(endpoint_role_t::kRoleServer));
  EXPECT_EQ(record_batch[kHTTPRespBodySizeIdx]->Get<types::Int64Value>(target_record_idx).val, 31);
}

TEST_F(GoHTTPTraceTest, LargePostMessage) {
  StartTransferDataThread();

  // Uncomment to enable tracing:
  // FLAGS_stirling_conn_trace_pid = go_http_fixture_.server_pid();

  go_http_fixture_.LaunchPostClient();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  // By default, we do not trace the client.
  EXPECT_THAT(
      testing::FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, go_http_fixture_.client_pid()),
      IsEmpty());

  // We do expect to trace the server.
  const std::vector<size_t> target_record_indices =
      testing::FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, go_http_fixture_.server_pid());
  ASSERT_THAT(target_record_indices, SizeIs(1));

  const size_t target_record_idx = target_record_indices.front();

  EXPECT_THAT(
      std::string(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      AllOf(HasSubstr(R"("Accept-Encoding":"gzip")"),
            HasSubstr(absl::Substitute(R"(Host":"localhost:$0")", go_http_fixture_.server_port())),
            ContainsRegex(R"(User-Agent":"Go-http-client/.+")")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(target_record_idx)),
      StrEq(
          "{\"data\":"
          "\"XVlBzgbaiCMRAjWwhTHctcuAxhxKQFDaFpLSjFbcXoEFfRsWxPLDnJObCsNVlgTeMaPEZQleQYhYzRyWJjPjzp"
          "fRFEgmotaFetHsbZRjxAwnwekrBEmfdzdcEkXBAkjQZLCtTMtTCoaNatyyiNKAReKJyiXJrscctNswYNsGRussVm"
          "aozFZBsbOJiFQGZsnwTKSmVoiGLOpbUOpEdKupdOMeRVjaRzLNTXYeUCWKsXbGyRAOmBTvKSJfjzaLbtZsyMGeuD"
          "tRzQMDQiYCOhgHOvgSeycJPJHYNufNjJhhjUVRuSqfgqVMkPYVkURUpiFvIZRgBmyArKCtzkjkZIvaBjMkXVbWGv"
          "bqzgexyALBsdjSGpngCwFkDifIBuufFMoWdiTskZoQJMqrTICTojIYxyeSxZyfroRODMbNDRZnPNRWCJPMHDtJmH"
          "AYORsUfUMApsVgzHblmYYtEjVgwfFbbGGcnqbaEREunUZjQXmZOtaRLUtmYgmSVYB... [TRUNCATED]"));
  EXPECT_THAT(record_batch[kHTTPReqBodySizeIdx]->Get<types::Int64Value>(target_record_idx).val,
              131096);
}

struct TraceRoleTestParam {
  endpoint_role_t role;
  size_t client_records_count;
  size_t server_records_count;
};

class TraceRoleTest : public GoHTTPTraceTest,
                      public ::testing::WithParamInterface<TraceRoleTestParam> {};

TEST_P(TraceRoleTest, VerifyRecordsCount) {
  const TraceRoleTestParam& param = GetParam();

  auto* socket_trace_connector = static_cast<SocketTraceConnector*>(source_.get());
  ASSERT_NE(nullptr, socket_trace_connector);
  EXPECT_OK(socket_trace_connector->UpdateBPFProtocolTraceRole(kProtocolHTTP, param.role));

  StartTransferDataThread();

  go_http_fixture_.LaunchGetClient();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);

  std::vector<size_t> client_record_ids;
  std::vector<size_t> server_record_ids;
  if (!tablets.empty()) {
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

    client_record_ids =
        testing::FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, go_http_fixture_.client_pid());

    server_record_ids =
        testing::FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, go_http_fixture_.server_pid());
    PX_LOG_VAR(PrintHTTPTable(record_batch));
  }

  EXPECT_THAT(client_record_ids, SizeIs(param.client_records_count));
  EXPECT_THAT(server_record_ids, SizeIs(param.server_records_count));
}

INSTANTIATE_TEST_SUITE_P(AllTraceRoles, TraceRoleTest,
                         ::testing::Values(TraceRoleTestParam{kRoleUnknown, 0, 0},
                                           TraceRoleTestParam{kRoleClient, 0, 0},
                                           TraceRoleTestParam{kRoleServer, 0, 1}));

// TODO(yzhao): Trace role only takes effect in BPF. With user-space filtering, i.e., intra-cluster
// events are discarded, this test no longer works for kRoleServer and kRoleAll. Add test for those
// two cases.

}  // namespace stirling
}  // namespace px
