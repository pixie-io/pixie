#include "src/stirling/jvm_stats_connector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/common/base/test_utils.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_environment.h"
#include "src/stirling/jvm_stats_table.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::ColWrapperSizeIs;
using ::pl::stirling::testing::FindRecordIdxMatchesPid;
using ::testing::Each;
using ::testing::SizeIs;

class JVMStatsConnectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char kClassPath[] = "src/stirling/testing/java/HelloWorld.jar";
    const std::string class_path = TestEnvironment::PathToTestDataFile(kClassPath);

    ASSERT_OK(hello_world_.Start({"java", "-cp", class_path, "-Xms1m", "-Xmx4m", "HelloWorld"}));
    // Give some time for the JVM process to write the data file.
    sleep(2);

    connector_ = JVMStatsConnector::Create("jvm_stats_connector");
    ASSERT_OK(connector_->Init());
    constexpr uint32_t kASID = 1;
    ctx_ = std::make_unique<ConnectorContext>(std::make_shared<md::AgentMetadataState>(kASID));
  }

  void TearDown() override {
    EXPECT_OK(connector_->Stop());
    hello_world_.Kill();
    EXPECT_EQ(9, hello_world_.Wait()) << "Server should have been killed.";
  }

  std::unique_ptr<SourceConnector> connector_;
  std::unique_ptr<ConnectorContext> ctx_;
  DataTable data_table_{kJVMStatsTable};

  SubProcess hello_world_;
};

// NOTE: This test will likely break under --runs_per_tests=100 or higher because of limitations of
// Bazel's sandboxing.
//
// Bazel uses PID namespace, so the PID of the java subprocess is often the same in different test
// runs. However, Bazel does not uses chroot, or other mechanisms of isolating filesystems. So the
// Java subprocesses all writes to the same memory mapped file with the same path, which causes data
// corruption and test failures.
TEST_F(JVMStatsConnectorTest, CaptureData) {
  connector_->TransferData(ctx_.get(), JVMStatsConnector::kTableNum, &data_table_);
  const types::ColumnWrapperRecordBatch& record_batch = *data_table_.ActiveRecordBatch();
  auto idxes = FindRecordIdxMatchesPid(record_batch, kUPIDIdx, hello_world_.child_pid());
  ASSERT_THAT(idxes, SizeIs(1));

  auto idx = idxes[0];

  EXPECT_GE(record_batch[kYoungGCTimeIdx]->Get<types::Duration64NSValue>(idx), 0);
  EXPECT_GE(record_batch[kFullGCTimeIdx]->Get<types::Duration64NSValue>(idx), 0);
  EXPECT_GE(record_batch[kUsedHeapSizeIdx]->Get<types::Int64Value>(idx).val, 0);
  EXPECT_GE(record_batch[kTotalHeapSizeIdx]->Get<types::Int64Value>(idx).val, 0);
  // This is derived from -Xmx4m. But we don't know how to control total_heap_size.
  EXPECT_GE(record_batch[kMaxHeapSizeIdx]->Get<types::Int64Value>(idx).val, 4 * 1024 * 1024);
}

}  // namespace stirling
}  // namespace pl
