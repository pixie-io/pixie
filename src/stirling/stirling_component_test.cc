#include <gtest/gtest.h>

#include "absl/strings/str_split.h"
#include "src/common/types/types.pb.h"
#include "src/stirling/data_table.h"
#include "src/stirling/proc_stat_connector.h"
#include "src/stirling/proto/collector_config.pb.h"
#include "src/stirling/pub_sub_manager.h"
#include "src/stirling/stirling.h"

namespace pl {
namespace stirling {

class StirlingComponentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_ = std::make_unique<SourceRegistry>();
    registry_->RegisterOrDie<FakeProcStatConnector>("test_connector");
    data_collector_ = std::make_unique<Stirling>(std::move(registry_));
  }

  std::unique_ptr<Stirling> data_collector_;
  std::unique_ptr<SourceRegistry> registry_;
};

TEST_F(StirlingComponentTest, registry_to_subscribe_test) {
  EXPECT_OK(data_collector_->Init());
  auto publish_proto = data_collector_->GetPublishProto();
  EXPECT_EQ(1, publish_proto.published_info_classes_size());
  auto subscribe_proto = SubscribeToAllElements(publish_proto);
  EXPECT_EQ(1, subscribe_proto.subscribed_info_classes_size());
  EXPECT_OK(data_collector_->SetSubscription(subscribe_proto));
  for (int i = 0; i < subscribe_proto.subscribed_info_classes_size(); ++i) {
    auto info_class = subscribe_proto.subscribed_info_classes(i);
    for (int j = 0; j < info_class.elements_size(); ++j) {
      auto element = info_class.elements(j);
      EXPECT_EQ(Element_State::Element_State_SUBSCRIBED, element.state());
    }
  }
}

class SourceToTableTest : public ::testing::Test {
 protected:
  SourceToTableTest() : info_class_mgr_("proc_stats_mgr") {}
  void SetUp() override {
    fake_proc_stat_ = FakeProcStatConnector::Create("fake_proc_stat_source");
    info_class_mgr_.SetSourceConnector(fake_proc_stat_.get());
    EXPECT_OK(fake_proc_stat_->PopulateSchema(&info_class_mgr_));
    // Need to set all the Element states to subscribe manually here
    // since we are not doing a pub sub here.
    for (size_t i = 0; i < info_class_mgr_.Schema().size(); ++i) {
      info_class_mgr_.UpdateElementSubscription(i, Element_State::Element_State_SUBSCRIBED);
    }
    table_ = std::make_unique<ColumnWrapperDataTable>(info_class_mgr_.Schema());
  }

  InfoClassSchema elements_;
  std::unique_ptr<SourceConnector> fake_proc_stat_;
  InfoClassManager info_class_mgr_;
  std::unique_ptr<DataTable> table_;
};

TEST_F(SourceToTableTest, source_to_table) {
  EXPECT_OK(fake_proc_stat_->Init());
  RawDataBuf r = fake_proc_stat_->GetData();
  EXPECT_EQ(1, r.num_records);
  EXPECT_OK(table_->AppendData(r.buf, r.num_records));
  auto record_batches_uptr = table_->GetColumnWrapperRecordBatches();
  auto record_batches_ptr = record_batches_uptr.ValueOrDie().get();
  ASSERT_TRUE(record_batches_ptr != nullptr);
  for (const auto& record_batch : *record_batches_ptr) {
    auto col_arrays = record_batch.get();
    ASSERT_TRUE(col_arrays != nullptr);
    ASSERT_EQ(DataType::INT64, (*col_arrays)[0]->DataType());
    auto col0 = std::static_pointer_cast<carnot::udf::Int64ValueColumnWrapper>((*col_arrays)[0]);
    auto col0_val = (*col0)[0].val;
    PL_UNUSED(col0_val);

    ASSERT_EQ(DataType::FLOAT64, (*col_arrays)[1]->DataType());
    auto col1 = std::static_pointer_cast<carnot::udf::Float64ValueColumnWrapper>((*col_arrays)[1]);
    auto col1_val = (*col1)[0].val;
    EXPECT_DOUBLE_EQ(70.0, col1_val);

    ASSERT_EQ(DataType::FLOAT64, (*col_arrays)[2]->DataType());
    auto col2 = std::static_pointer_cast<carnot::udf::Float64ValueColumnWrapper>((*col_arrays)[2]);
    auto col2_val = (*col2)[0].val;
    EXPECT_DOUBLE_EQ(10.0, col2_val);

    ASSERT_EQ(DataType::FLOAT64, (*col_arrays)[3]->DataType());
    auto col3 = std::static_pointer_cast<carnot::udf::Float64ValueColumnWrapper>((*col_arrays)[3]);
    auto col3_val = (*col3)[0].val;
    EXPECT_DOUBLE_EQ(20.0, col3_val);
  }
}

}  // namespace stirling
}  // namespace pl
