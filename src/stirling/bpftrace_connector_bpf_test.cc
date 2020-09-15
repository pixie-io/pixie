#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string_view>

#include "src/common/testing/testing.h"

#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/data_table.h"

namespace pl {
namespace stirling {

class BPFTraceConnectorBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    source_ = CPUStatBPFTraceConnector::Create("cpu_stats");
    ASSERT_OK(source_->Init());

    ctx_ = std::make_unique<StandaloneContext>();

    source_->InitContext(ctx_.get());

    // InitContext will cause Uprobes to deploy.
    // It won't return until the first set of uprobes has successfully deployed.
    // Sleep an additional second just to be safe.
    sleep(1);
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  const DataTableSchema& kTable = CPUStatBPFTraceConnector::kTable;
  const int kTableNum = 0;

  std::unique_ptr<SourceConnector> source_;
  std::unique_ptr<StandaloneContext> ctx_;
};

TEST_F(BPFTraceConnectorBPFTest, Basic) {
  sleep(5);

  DataTable data_table(kTable);
  source_->TransferData(ctx_.get(), kTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
}

}  // namespace stirling
}  // namespace pl
