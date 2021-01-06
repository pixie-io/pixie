#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/testing/testing.h"

namespace pl {
namespace stirling {

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

class RedisTraceBPFTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  RedisTraceBPFTest() { PL_CHECK_OK(container_.Run(150, {})); }

  RedisContainer container_;
};

// A place holder to verify the setup of the redis and redis_cli container.
TEST_F(RedisTraceBPFTest, VerifyFlushAllCommand) {
  const std::string redis_cli_cmd =
      absl::Substitute("docker run --rm --network=container:$0 redis redis-cli flushall",
                       container_.container_name());
  ASSERT_OK_AND_ASSIGN(const std::string output, pl::Exec(redis_cli_cmd));
  EXPECT_THAT(output, StrEq("OK\n"));

  DataTable data_table(kRedisTable);
  source_->TransferData(ctx_.get(), SocketTraceConnector::kRedisTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();

  ASSERT_FALSE(tablets.empty());

  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  const std::vector<size_t> target_record_indices =
      testing::FindRecordIdxMatchesPID(record_batch, kRedisUPIDIdx, container_.process_pid());
  ASSERT_THAT(target_record_indices, SizeIs(1));

  EXPECT_THAT(
      std::string(record_batch[kRedisReqIdx]->Get<types::StringValue>(target_record_indices[0])),
      StrEq("[flushall]"));
  EXPECT_THAT(
      std::string(record_batch[kRedisRespIdx]->Get<types::StringValue>(target_record_indices[0])),
      StrEq("OK"));
}

}  // namespace stirling
}  // namespace pl
