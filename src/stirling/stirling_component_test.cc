#include <absl/strings/str_split.h>

#include "src/common/testing/testing.h"
#include "src/shared/types/proto/types_pb_wrapper.h"
#include "src/stirling/data_table.h"
#include "src/stirling/proc_stat_connector.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/pub_sub_manager.h"
#include "src/stirling/stirling.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

class StirlingComponentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_ = std::make_unique<SourceRegistry>();
    registry_->RegisterOrDie<FakeProcStatConnector>("test_connector");
    data_collector_ = Stirling::Create(std::move(registry_));
  }

  std::unique_ptr<Stirling> data_collector_;
  std::unique_ptr<SourceRegistry> registry_;
};

TEST_F(StirlingComponentTest, registry_to_subscribe_test) {
  // Generate the Publish message.
  stirlingpb::Publish publish_proto;
  data_collector_->GetPublishProto(&publish_proto);
  EXPECT_FALSE(publish_proto.published_info_classes(0).subscribed());
  EXPECT_EQ(1, publish_proto.published_info_classes_size());

  // Subscribe to all Info Classes in the publish message.
  auto subscribe_proto = SubscribeToAllInfoClasses(publish_proto);
  EXPECT_EQ(1, subscribe_proto.subscribed_info_classes_size());
  EXPECT_TRUE(subscribe_proto.subscribed_info_classes(0).subscribed());
  EXPECT_OK(data_collector_->SetSubscription(subscribe_proto));
  for (int i = 0; i < subscribe_proto.subscribed_info_classes_size(); ++i) {
    const stirlingpb::InfoClass& info_class = subscribe_proto.subscribed_info_classes(i);
    EXPECT_TRUE(info_class.subscribed());
  }
}

class SourceToTableTest : public ::testing::Test {
 protected:
  SourceToTableTest() : info_class_mgr_(FakeProcStatConnector::kTable) {}
  void SetUp() override {
    fake_proc_stat_ = FakeProcStatConnector::Create("fake_proc_stat_source");
    info_class_mgr_.SetSourceConnector(fake_proc_stat_.get(), /* table_num */ 0);
    table_ = std::make_unique<DataTable>(info_class_mgr_.Schema());
  }

  std::unique_ptr<SourceConnector> fake_proc_stat_;
  InfoClassManager info_class_mgr_;
  std::unique_ptr<DataTable> table_;
};

TEST_F(SourceToTableTest, source_to_table) {
  EXPECT_OK(fake_proc_stat_->Init());
  uint32_t table_num = 0;

  StandaloneContext ctx;
  fake_proc_stat_->TransferData(&ctx, table_num, table_.get());
  auto record_batches = table_->ConsumeRecords();
  for (const auto& record_batch : record_batches) {
    auto& columns = record_batch.records;

    ASSERT_EQ(types::DataType::TIME64NS, columns[0]->data_type());
    auto col0_val = columns[0]->Get<types::Time64NSValue>(0).val;
    PL_UNUSED(col0_val);

    ASSERT_EQ(types::DataType::FLOAT64, columns[1]->data_type());
    auto col1_val = columns[1]->Get<types::Float64Value>(0).val;
    EXPECT_DOUBLE_EQ(70.0, col1_val);

    ASSERT_EQ(types::DataType::FLOAT64, columns[2]->data_type());
    auto col2_val = columns[2]->Get<types::Float64Value>(0).val;
    EXPECT_DOUBLE_EQ(10.0, col2_val);

    ASSERT_EQ(types::DataType::FLOAT64, columns[3]->data_type());
    auto col3_val = columns[3]->Get<types::Float64Value>(0).val;
    EXPECT_DOUBLE_EQ(20.0, col3_val);
  }
}

}  // namespace stirling
}  // namespace pl
