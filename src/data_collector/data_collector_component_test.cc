#include <gtest/gtest.h>

#include "absl/strings/str_split.h"
#include "src/common/types/types.pb.h"
#include "src/data_collector/data_table.h"
#include "src/data_collector/proc_stat_connector.h"

namespace pl {
namespace datacollector {

class FakeProcStatConnector : public ProcStatConnector {
 public:
  FakeProcStatConnector() = delete;
  explicit FakeProcStatConnector(const std::string& source_name,
                                 const std::vector<InfoClassElement> elements)
      : ProcStatConnector(source_name, elements) {}
  ~FakeProcStatConnector() = default;

 protected:
  std::vector<std::string> GetProcParams() override {
    std::string stats = "cpu  ";
    std::vector<std::string> parsed_str;
    for (int i = 0; i < kNumCPUStatFields; ++i) {
      stats += std::to_string(fake_stat_ + i) + " ";
    }
    fake_stat_++;
    parsed_str = absl::StrSplit(stats, ' ', absl::SkipWhitespace());
    return parsed_str;
  }

 private:
  int fake_stat_ = 0;
};

class SourceConnectorTest : public ::testing::Test {
 protected:
  SourceConnectorTest()
      : elements_({InfoClassElement("_time", DataType::INT64,
                                    Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
                   InfoClassElement("system_percent", DataType::FLOAT64,
                                    Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
                   InfoClassElement("user_percent", DataType::FLOAT64,
                                    Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
                   InfoClassElement("idle_percent", DataType::FLOAT64,
                                    Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED)}),
        fake_proc_stat_("proc_stats", elements_),
        schema_("proc_stats_schema") {}

  void SetUp() override {
    source_ptr_ = &fake_proc_stat_;
    schema_.SetSourceConnector(source_ptr_);
    EXPECT_OK(fake_proc_stat_.PopulateSchema(&schema_));
    table_ = std::make_unique<ColumnWrapperDataTable>(schema_);
  }

  std::vector<InfoClassElement> elements_;
  FakeProcStatConnector fake_proc_stat_;
  InfoClassSchema schema_;
  SourceConnector* source_ptr_;
  std::unique_ptr<DataTable> table_;
};

TEST_F(SourceConnectorTest, source_to_table) {
  EXPECT_OK(fake_proc_stat_.Init());
  RawDataBuf r = fake_proc_stat_.GetData();

  EXPECT_EQ(1, r.num_records);
  EXPECT_OK(table_->AppendData(r.buf, r.num_records));

  auto record_batches_uptr = table_->GetColumnWrapperRecordBatches();
  auto record_batches_ptr = record_batches_uptr.ValueOrDie().get();
  for (const auto& record_batch : *record_batches_ptr) {
    auto col_arrays = record_batch.get();

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

}  // namespace datacollector
}  // namespace pl
