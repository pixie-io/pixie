#include "src/stirling/jvm_stats_connector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/stirling/jvm_stats_table.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::ColWrapperSizeIs;
using ::testing::Each;

class JVMStatsConnectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    connector_ = JVMStatsConnector::Create("jvm_stats_connector");
    ASSERT_OK(connector_->Init());
    constexpr uint32_t kASID = 1;
    ctx_ = std::make_unique<ConnectorContext>(std::make_shared<md::AgentMetadataState>(kASID));
  }

  void TearDown() override { ASSERT_OK(connector_->Stop()); }

  std::unique_ptr<SourceConnector> connector_;
  std::unique_ptr<ConnectorContext> ctx_;
  DataTable data_table_{kJVMStatsTable};
};

TEST_F(JVMStatsConnectorTest, CaptureData) {
  connector_->TransferData(ctx_.get(), JVMStatsConnector::kTableNum, &data_table_);
  const types::ColumnWrapperRecordBatch& record_batch = *data_table_.ActiveRecordBatch();
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
}

}  // namespace stirling
}  // namespace pl
