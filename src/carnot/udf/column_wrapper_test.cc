#include <arrow/array.h>
#include <arrow/buffer.h>
#include <arrow/builder.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include "src/carnot/udf/column_wrapper.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace udf {

TEST(ColumnWrapperTest, make_test_bool) {
  auto wrapper = ColumnWrapper::Make(UDFDataType::BOOLEAN, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(UDFDataType::BOOLEAN, wrapper->DataType());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(UDFDataTypeTraits<UDFDataType::BOOLEAN>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapperTest, make_test_int64) {
  auto wrapper = ColumnWrapper::Make(UDFDataType::INT64, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(UDFDataType::INT64, wrapper->DataType());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(UDFDataTypeTraits<UDFDataType::INT64>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapperTest, make_test_float64) {
  auto wrapper = ColumnWrapper::Make(UDFDataType::FLOAT64, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(UDFDataType::FLOAT64, wrapper->DataType());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(UDFDataTypeTraits<UDFDataType::FLOAT64>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapperTest, make_test_string) {
  auto wrapper = ColumnWrapper::Make(UDFDataType::STRING, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(UDFDataType::STRING, wrapper->DataType());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(UDFDataTypeTraits<UDFDataType::STRING>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapper, FromArrowBool) {
  arrow::BooleanBuilder builder;
  PL_CHECK_OK(builder.Append(true));
  PL_CHECK_OK(builder.Append(true));
  PL_CHECK_OK(builder.Append(false));

  std::shared_ptr<arrow::Array> arr;
  PL_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->Equals(arr));
}

TEST(ColumnWrapper, FromArrowInt64) {
  arrow::Int64Builder builder;
  PL_CHECK_OK(builder.Append(1));
  PL_CHECK_OK(builder.Append(2));
  PL_CHECK_OK(builder.Append(3));

  std::shared_ptr<arrow::Array> arr;
  PL_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->Equals(arr));
}

TEST(ColumnWrapper, FromArrowFloat64) {
  arrow::DoubleBuilder builder;
  PL_CHECK_OK(builder.Append(1));
  PL_CHECK_OK(builder.Append(2));
  PL_CHECK_OK(builder.Append(3));

  std::shared_ptr<arrow::Array> arr;
  PL_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->ApproxEquals(arr));
}

TEST(ColumnWrapper, FromArrowString) {
  arrow::StringBuilder builder;
  PL_CHECK_OK(builder.Append("abc"));
  PL_CHECK_OK(builder.Append("def"));
  PL_CHECK_OK(builder.Append("hello"));

  std::shared_ptr<arrow::Array> arr;
  PL_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->Equals(arr));
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
