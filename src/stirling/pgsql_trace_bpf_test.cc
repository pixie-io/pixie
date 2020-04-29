#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <absl/strings/str_split.h>

#include <string>
#include <string_view>

#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/stirling/output.h"
#include "src/stirling/pgsql_table.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::AccessRecordBatch;
using ::pl::stirling::testing::FindRecordIdxMatchesPid;
using ::testing::HasSubstr;
using ::testing::SizeIs;
using ::testing::StrEq;

class PostgreSQLContainer : public ContainerRunner {
 public:
  PostgreSQLContainer() : ContainerRunner(kImageName, kContainerNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImageName = "postgres";
  static constexpr std::string_view kContainerNamePrefix = "postgres_testing";
  static constexpr std::string_view kReadyMessage =
      "database system is ready to accept connections";
};

class PostgreSQLTraceTest : public testing::SocketTraceBPFTest {
 protected:
  PostgreSQLTraceTest() {
    const char kCreateNetCmd[] = "docker network create --driver bridge pg-net";
    PL_CHECK_OK(pl::Exec(kCreateNetCmd));

    PL_CHECK_OK(container_.Run(150, {"--network=pg-net", "--env=POSTGRES_PASSWORD=docker"}));
  }
  ~PostgreSQLTraceTest() { container_.Stop(); }

  DataTable data_table_{kPGSQLTable};

  PostgreSQLContainer container_;
};

// TODO(yzhao): We want to test Stirling's behavior when intercept the middle of the traffic.
// One way is to let SubProcess able to accept STDIN after launching. This test does not have that
// capability because it's running a query from start to finish, which always establish new
// connections.
TEST_F(PostgreSQLTraceTest, SelectQuery) {
  // --pid host is required to access the correct PID.
  constexpr char kCmdTmpl[] =
      "docker run --pid host --rm -e PGPASSWORD=docker --network pg-net postgres bash -c "
      R"('psql -h $0 -U postgres -c "$1" &>/dev/null & echo $$! && wait')";
  const std::string kCreateTableCmd =
      absl::Substitute(kCmdTmpl, container_.container_name(),
                       "create table foo (field0 serial primary key);"
                       "insert into foo values (12345);"
                       "select * from foo;");
  ASSERT_OK_AND_ASSIGN(const std::string create_table_output, pl::Exec(kCreateTableCmd));
  int32_t client_pid;
  ASSERT_TRUE(absl::SimpleAtoi(create_table_output, &client_pid));

  source_->TransferData(ctx_.get(), SocketTraceConnector::kPGSQLTableNum, &data_table_);

  const types::ColumnWrapperRecordBatch& record_batch = *data_table_.ActiveRecordBatch();
  auto indices = FindRecordIdxMatchesPid(record_batch, kPGSQLUPIDIdx, client_pid);
  ASSERT_THAT(indices, SizeIs(1));

  EXPECT_THAT(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLReqIdx, indices[0]),
              HasSubstr("create table foo (field0 serial primary key);"
                        "insert into foo values (12345);"
                        "select * from foo;"));
  EXPECT_THAT(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLRespIdx, indices[0]),
              // TODO(yzhao): This is a bug, it should return output for the other 2 queries.
              StrEq("CREATE TABLE"));
}

}  // namespace stirling
}  // namespace pl
