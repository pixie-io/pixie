#include <filesystem>

#include "src/common/testing/testing.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/protocols/http/testing/go_http_fixture.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/testing.h"

namespace pl {
namespace stirling {

using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

class GoHTTPTraceTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  GoHTTPTraceTest() : SocketTraceBPFTest() {}

  void SetUp() override {
    SocketTraceBPFTest::SetUp();
    go_http_fixture_.LaunchServer();
  }

  void TearDown() override {
    SocketTraceBPFTest::TearDown();
    go_http_fixture_.ShutDown();
  }

  testing::GoHTTPFixture go_http_fixture_;

  // Create a context to pass into each TransferData() in the test, using a dummy ASID.
  static constexpr uint32_t kASID = 1;

  DataTable data_table_{kHTTPTable};
};

TEST_F(GoHTTPTraceTest, RequestAndResponse) {
  go_http_fixture_.LaunchClient();

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table_);
  std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

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
  EXPECT_THAT(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(target_record_idx),
              StrEq(absl::StrCat(R"({"greeter":"Hello PixieLabs!"})", "\n")));
  // This test currently performs client-side tracing because of the cluster CIDR in
  // socket_trace_bpf_test_fixture.h.
  EXPECT_EQ(record_batch[kHTTPTraceRoleIdx]->Get<types::Int64Value>(target_record_idx).val,
            static_cast<int>(EndpointRole::kRoleServer));
  EXPECT_EQ(record_batch[kHTTPRespBodySizeIdx]->Get<types::Int64Value>(target_record_idx).val, 31);
}

struct TraceRoleTestParam {
  EndpointRole role;
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

  go_http_fixture_.LaunchClient();

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table_);
  std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();

  std::vector<size_t> client_record_ids;
  std::vector<size_t> server_record_ids;
  if (!tablets.empty()) {
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

    client_record_ids =
        testing::FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, go_http_fixture_.client_pid());

    server_record_ids =
        testing::FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, go_http_fixture_.server_pid());
  }

  EXPECT_THAT(client_record_ids, SizeIs(param.client_records_count));
  EXPECT_THAT(server_record_ids, SizeIs(param.server_records_count));
}

INSTANTIATE_TEST_SUITE_P(AllTraceRoles, TraceRoleTest,
                         ::testing::Values(TraceRoleTestParam{kRoleNone, 0, 0},
                                           TraceRoleTestParam{kRoleServer, 0, 1}));

// TODO(yzhao): Trace role only takes effect in BPF. With user-space filtering, i.e., intra-cluster
// events are discarded, this test no longer works for kRoleServer and kRoleAll. Add test for those
// two cases.

}  // namespace stirling
}  // namespace pl
