#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::testing::ElementsAre;
using ::testing::SizeIs;
using ::testing::StrEq;

class RedisContainer : public ContainerRunner {
 public:
  RedisContainer() : ContainerRunner(kImageName, kContainerNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImageName = "redis";
  static constexpr std::string_view kContainerNamePrefix = "redis_test";
  static constexpr std::string_view kReadyMessage = "# Server initialized";
};

struct RedisTraceTestCase {
  std::string cmd;
  std::string req;
  std::string resp;
};

class RedisTraceBPFTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false>,
                          public ::testing::WithParamInterface<RedisTraceTestCase> {
 protected:
  RedisTraceBPFTest() { PL_CHECK_OK(container_.Run(150, {})); }

  RedisContainer container_;
};

struct RedisTraceRecord {
  std::string req;
  std::string resp;
};

std::ostream& operator<<(std::ostream& os, const RedisTraceRecord& record) {
  os << "req: " << record.req << " resp: " << record.resp << std::endl;
  return os;
}

bool operator==(const RedisTraceRecord& lhs, const RedisTraceRecord& rhs) {
  return lhs.req == rhs.req && lhs.resp == rhs.resp;
}

std::vector<RedisTraceRecord> GetRedisTraceRecords(
    const types::ColumnWrapperRecordBatch& record_batch) {
  std::vector<RedisTraceRecord> res;
  for (size_t i = 0; i < record_batch[kRedisReqIdx]->Size(); ++i) {
    res.push_back(
        RedisTraceRecord{std::string(record_batch[kRedisReqIdx]->Get<types::StringValue>(i)),
                         std::string(record_batch[kRedisRespIdx]->Get<types::StringValue>(i))});
  }
  return res;
}

// Verifies that batched commands can be traced correctly.
TEST_F(RedisTraceBPFTest, VerifyBatchedCommands) {
  constexpr std::string_view kRedisDockerCmdTmpl =
      R"(docker run --rm --network=container:$0 redis bash -c "echo '$1' | redis-cli")";
  constexpr std::string_view kRedisCmds = R"(
    set foo 100
    bitcount foo 0 0
    incr foo
    append foo xxx
    get foo
  )";
  const std::string redis_cli_cmd =
      absl::Substitute(kRedisDockerCmdTmpl, container_.container_name(), kRedisCmds);
  ASSERT_OK_AND_ASSIGN(const std::string output, pl::Exec(redis_cli_cmd));
  ASSERT_FALSE(output.empty());

  DataTable data_table(kRedisTable);
  source_->TransferData(ctx_.get(), SocketTraceConnector::kRedisTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();

  ASSERT_FALSE(tablets.empty());

  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  std::vector<RedisTraceRecord> redis_trace_records = GetRedisTraceRecords(record_batch);

  // redis-cli sends a 'command' req to query all available commands from server.
  // The response is too long to test meaningfully, so we ignore them.
  redis_trace_records.erase(redis_trace_records.begin());

  EXPECT_THAT(redis_trace_records, ElementsAre(RedisTraceRecord{"[set, foo, 100]", "OK"},
                                               RedisTraceRecord{"[bitcount, foo, 0, 0]", "3"},
                                               RedisTraceRecord{"[incr, foo]", "101"},
                                               RedisTraceRecord{"[append, foo, xxx]", "6"},
                                               RedisTraceRecord{"[get, foo]", "101xxx"}));
}

// Verifies individual commands.
TEST_P(RedisTraceBPFTest, VerifyCommand) {
  std::string_view redis_cmd = GetParam().cmd;

  std::string redis_cli_cmd =
      absl::Substitute("docker run --rm --network=container:$0 redis redis-cli $1",
                       container_.container_name(), redis_cmd);
  ASSERT_OK_AND_ASSIGN(const std::string output, pl::Exec(redis_cli_cmd));
  ASSERT_FALSE(output.empty());

  DataTable data_table(kRedisTable);
  source_->TransferData(ctx_.get(), SocketTraceConnector::kRedisTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();

  ASSERT_FALSE(tablets.empty());

  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  EXPECT_THAT(GetRedisTraceRecords(record_batch),
              ElementsAre(RedisTraceRecord{GetParam().req, GetParam().resp}));
}

INSTANTIATE_TEST_SUITE_P(
    Commands, RedisTraceBPFTest,
    // Add new commands here.
    ::testing::Values(RedisTraceTestCase{"lpush ilist 100", "[lpush, ilist, 100]", "1"},
                      RedisTraceTestCase{"rpush ilist 200", "[rpush, ilist, 200]", "1"},
                      RedisTraceTestCase{"lrange ilist 0 1", "[lrange, ilist, 0, 1]", "[]"},
                      RedisTraceTestCase{"flushall", "[flushall]", "OK"}));

}  // namespace stirling
}  // namespace pl
