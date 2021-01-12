#include <absl/strings/str_split.h>

#include "src/common/testing/testing.h"
#include "src/shared/types/proto/wrapper/types_pb_wrapper.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/types.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/seq_gen/seq_gen_connector.h"
#include "src/stirling/stirling.h"

namespace pl {
namespace stirling {

class StirlingComponentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_ = std::make_unique<SourceRegistry>();
    registry_->RegisterOrDie<SeqGenConnector>("test_connector");
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
  EXPECT_EQ(2, publish_proto.published_info_classes_size());

  // Subscribe to all Info Classes in the publish message.
  auto subscribe_proto = SubscribeToAllInfoClasses(publish_proto);
  EXPECT_EQ(2, subscribe_proto.subscribed_info_classes_size());
  EXPECT_TRUE(subscribe_proto.subscribed_info_classes(0).subscribed());
  EXPECT_OK(data_collector_->SetSubscription(subscribe_proto));
  for (int i = 0; i < subscribe_proto.subscribed_info_classes_size(); ++i) {
    const stirlingpb::InfoClass& info_class = subscribe_proto.subscribed_info_classes(i);
    EXPECT_TRUE(info_class.subscribed());
  }
}

class SourceToTableTest : public ::testing::Test {
 protected:
  SourceToTableTest() : info_class_mgr_(SeqGenConnector::kSeq0Table) {}
  void SetUp() override {
    source_ = SeqGenConnector::Create("dummy_sources");
    dynamic_cast<SeqGenConnector*>(source_.get())->ConfigureNumRowsPerGet(1);
    info_class_mgr_.SetSourceConnector(source_.get(), /* table_num */ 0);
    table_ = std::make_unique<DataTable>(info_class_mgr_.Schema());
  }

  std::unique_ptr<SourceConnector> source_;
  InfoClassManager info_class_mgr_;
  std::unique_ptr<DataTable> table_;
};

TEST_F(SourceToTableTest, source_to_table) {
  EXPECT_OK(source_->Init());
  uint32_t table_num = 0;

  StandaloneContext ctx;
  source_->TransferData(&ctx, table_num, table_.get());
  auto record_batches = table_->ConsumeRecords();
  for (const auto& record_batch : record_batches) {
    auto& columns = record_batch.records;

    ASSERT_EQ(types::DataType::TIME64NS, columns[0]->data_type());
    auto col0_val = columns[0]->Get<types::Time64NSValue>(0).val;
    PL_UNUSED(col0_val);

    ASSERT_EQ(types::DataType::INT64, columns[1]->data_type());
    auto col1_val = columns[1]->Get<types::Int64Value>(0).val;
    EXPECT_EQ(1, col1_val);

    ASSERT_EQ(types::DataType::INT64, columns[2]->data_type());
    auto col2_val = columns[2]->Get<types::Int64Value>(0).val;
    EXPECT_EQ(0, col2_val);

    ASSERT_EQ(types::DataType::INT64, columns[3]->data_type());
    auto col3_val = columns[3]->Get<types::Int64Value>(0).val;
    EXPECT_EQ(0, col3_val);

    ASSERT_EQ(types::DataType::INT64, columns[4]->data_type());
    auto col4_val = columns[4]->Get<types::Int64Value>(0).val;
    EXPECT_EQ(1, col4_val);

    ASSERT_EQ(types::DataType::FLOAT64, columns[5]->data_type());
    auto col5_val = columns[5]->Get<types::Float64Value>(0).val;
    EXPECT_DOUBLE_EQ(0.0, col5_val);
  }
}

}  // namespace stirling
}  // namespace pl
