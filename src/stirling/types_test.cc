#include "src/stirling/types.h"

namespace pl {
namespace stirling {

TEST(InitRecordBatchTest, FieldsAreAdded) {
  types::ColumnWrapperRecordBatch record_batch;
  static constexpr DataElement elements_array[] = {
      DataElement("1st", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER),
      DataElement("2nd", types::DataType::INT64, types::PatternType::GENERAL),
      DataElement("3rd", types::DataType::STRING, types::PatternType::GENERAL),
  };
  constexpr ConstVectorView<DataElement> elements(elements_array);
  const int target_capacity = 100;
  EXPECT_OK(InitRecordBatch(elements, target_capacity, &record_batch));
  ASSERT_EQ(3, record_batch.size());
}

}  // namespace stirling
}  // namespace pl
