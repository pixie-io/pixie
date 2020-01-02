#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <experimental/filesystem>

#include "src/common/base/test_utils.h"
#include "src/common/exec/subprocess.h"
#include "src/stirling/data_table.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/common.h"

DEFINE_string(go_greeter_client_path, "", "The path to the go greeter client executable.");
DEFINE_string(go_greeter_server_path, "", "The path to the go greeter server executable.");

namespace pl {
namespace stirling {

using ::pl::stirling::testing::ColWrapperSizeIs;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ContainsRegex;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::MatchesRegex;
using ::testing::SizeIs;
using ::testing::StrEq;

namespace fs = std::experimental::filesystem;

constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;

class GoHTTPCTraceTest : public ::testing::Test {
 protected:
  GoHTTPCTraceTest()
      : data_table_(kHTTPTable),
        ctx_(std::make_unique<ConnectorContext>(std::make_shared<md::AgentMetadataState>(kASID))) {}

  void SetUp() override {
    connector_ = SocketTraceConnector::Create("socket_trace_connector");
    socket_trace_connector_ = static_cast<SocketTraceConnector*>(connector_.get());
    CHECK(socket_trace_connector_ != nullptr);
    PL_CHECK_OK(connector_->Init());

    CHECK(!FLAGS_go_greeter_client_path.empty())
        << "--go_greeter_client_path cannot be empty. You should run this test with bazel.";
    CHECK(fs::exists(fs::path(FLAGS_go_greeter_client_path))) << FLAGS_go_greeter_client_path;

    CHECK(!FLAGS_go_greeter_server_path.empty())
        << "--go_greeter_server_path cannot be empty. You should run this test with bazel.";
    CHECK(fs::exists(fs::path(FLAGS_go_greeter_server_path))) << FLAGS_go_greeter_server_path;

    server_path_ = FLAGS_go_greeter_server_path;
    client_path_ = FLAGS_go_greeter_client_path;

    PL_CHECK_OK(s_.Start({server_path_}));
  }

  void TearDown() override {
    s_.Kill();
    EXPECT_EQ(9, s_.Wait()) << "Server should have been killed.";
  }

  std::string server_path_;
  std::string client_path_;

  // Create a context to pass into each TransferData() in the test, using a dummy ASID.
  static constexpr uint32_t kASID = 1;

  DataTable data_table_;
  SubProcess c_;
  SubProcess s_;
  std::unique_ptr<ConnectorContext> ctx_;
  std::unique_ptr<SourceConnector> connector_;
  SocketTraceConnector* socket_trace_connector_;
};

TEST_F(GoHTTPCTraceTest, RequestAndResponse) {
  SubProcess c;
  EXPECT_OK(c.Start({client_path_, "-name=PixieLabs"}));

  EXPECT_OK(socket_trace_connector_->TestOnlySetTargetPID(c.child_pid()));

  EXPECT_EQ(0, c.Wait()) << "Client should exit normally.";

  connector_->TransferData(ctx_.get(), kHTTPTableNum, &data_table_);

  const types::ColumnWrapperRecordBatch& record_batch = *data_table_.ActiveRecordBatch();
  const std::vector<size_t> target_record_indices =
      testing::FindRecordIdxMatchesPid(record_batch, c.child_pid());
  ASSERT_THAT(target_record_indices, SizeIs(1));

  const size_t target_record_idx = target_record_indices.front();

  EXPECT_THAT(
      std::string(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      AllOf(HasSubstr(R"("Accept-Encoding":"gzip")"), HasSubstr(R"(Host":"localhost:50050")"),
            ContainsRegex(R"(User-Agent":"Go-http-client/.+")")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      AllOf(HasSubstr(R"("Content-Length":"31")"), HasSubstr(R"(Content-Type":"json)")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(target_record_idx)),
      HasSubstr("127.0.0.1"));
  EXPECT_EQ(50050, record_batch[kHTTPRemotePortIdx]->Get<types::Int64Value>(target_record_idx).val);
  EXPECT_THAT(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(target_record_idx),
              StrEq(absl::StrCat(R"({"greeter":"Hello PixieLabs!"})", "\n")));
}

}  // namespace stirling
}  // namespace pl
