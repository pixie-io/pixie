#include "src/stirling/types.h"

namespace pl {
namespace stirling {

TEST(InitRecordBatchTest, FieldsAreAdded) {
  types::ColumnWrapperRecordBatch record_batch;
  DataElements elements = {
      DataElement("1st", types::DataType::TIME64NS),
      DataElement("2nd", types::DataType::INT64),
      DataElement("3rd", types::DataType::STRING),
  };
  const int target_capacity = 100;
  EXPECT_OK(InitRecordBatch(elements, target_capacity, &record_batch));
  ASSERT_EQ(3, record_batch.size());
}

}  // namespace stirling
}  // namespace pl
