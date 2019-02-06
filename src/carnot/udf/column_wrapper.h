#pragma once

#include <arrow/array.h>
#include <arrow/buffer.h>
#include <arrow/builder.h>

#include <memory>
#include <vector>

#include "src/carnot/udf/arrow_adapter.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace udf {

class ColumnWrapper;
using SharedColumnWrapper = std::shared_ptr<ColumnWrapper>;

/**
 * Column wrapper stores underlying data so that it can be retrieved in a type erased way
 * to allow column chucks to be transparently passed.
 */
class ColumnWrapper {
 public:
  ColumnWrapper() = default;
  virtual ~ColumnWrapper() = default;

  static SharedColumnWrapper Make(UDFDataType data_type, size_t size);
  static SharedColumnWrapper FromArrow(const std::shared_ptr<arrow::Array> &arr);

  virtual UDFBaseValue *UnsafeRawData() = 0;
  virtual const UDFBaseValue *UnsafeRawData() const = 0;
  virtual UDFDataType DataType() const = 0;
  virtual size_t Size() const = 0;
  virtual std::shared_ptr<arrow::Array> ConvertToArrow(arrow::MemoryPool *mem_pool) = 0;
};

/**
 * Implementation of type erased vectors for a specific data type.
 * @tparam T The UDFValueType.
 */
template <typename T>
class ColumnWrapperTmpl : public ColumnWrapper {
 public:
  explicit ColumnWrapperTmpl(size_t size) : data_(size) {}
  explicit ColumnWrapperTmpl(size_t size, const T &val) : data_(size, val) {}
  explicit ColumnWrapperTmpl(const std::vector<T> &vals) : data_(vals) {}

  ~ColumnWrapperTmpl() = default;

  T *UnsafeRawData() override { return data_.data(); }
  const T *UnsafeRawData() const override { return data_.data(); }
  UDFDataType DataType() const override { return UDFValueTraits<T>::data_type; }

  size_t Size() const override { return data_.size(); }

  std::shared_ptr<arrow::Array> ConvertToArrow(arrow::MemoryPool *mem_pool) override {
    return ToArrow(data_, mem_pool);
  }

  T operator[](size_t idx) const { return data_[idx]; }

  void resize(size_t size) { data_.resize(size); }

  void clear() { data_.clear(); }

 private:
  std::vector<T> data_;
};

// PL_CARNOT_UPDATE_FOR_NEW_TYPES.
using BoolValueColumnWrapper = ColumnWrapperTmpl<BoolValue>;
using Int64ValueColumnWrapper = ColumnWrapperTmpl<Int64Value>;
using Float64ValueColumnWrapper = ColumnWrapperTmpl<Float64Value>;
using StringValueColumnWrapper = ColumnWrapperTmpl<StringValue>;

template <typename TColumnWrapper, types::DataType DType>
inline SharedColumnWrapper FromArrowImpl(const std::shared_ptr<arrow::Array> &arr) {
  CHECK_EQ(arr->type_id(), UDFDataTypeTraits<DType>::arrow_type_id);
  size_t size = arr->length();
  auto wrapper = TColumnWrapper::Make(DType, size);
  auto arr_casted =
      reinterpret_cast<typename UDFDataTypeTraits<DType>::arrow_array_type *>(arr.get());
  typename UDFDataTypeTraits<DType>::udf_value_type *out_data =
      reinterpret_cast<TColumnWrapper *>(wrapper.get())->UnsafeRawData();
  for (size_t i = 0; i < size; ++i) {
    out_data[i] = arr_casted->Value(i);
  }
  return wrapper;
}

template <>
inline SharedColumnWrapper FromArrowImpl<StringValueColumnWrapper, UDFDataType::STRING>(
    const std::shared_ptr<arrow::Array> &arr) {
  CHECK_EQ(arr->type_id(), UDFDataTypeTraits<types::STRING>::arrow_type_id);
  size_t size = arr->length();
  auto wrapper = StringValueColumnWrapper::Make(types::STRING, size);
  auto arr_casted = reinterpret_cast<arrow::StringArray *>(arr.get());
  StringValue *out_data =
      reinterpret_cast<StringValueColumnWrapper *>(wrapper.get())->UnsafeRawData();
  for (size_t i = 0; i < size; ++i) {
    out_data[i] = arr_casted->GetString(i);
  }
  return wrapper;
}

/**
 * Create a type erased ColumnWrapper from an ArrowArray.
 * @param arr the arrow array.
 * @return A shared_ptr to the ColumnWrapper.
 * PL_CARNOT_UPDATE_FOR_NEW_TYPES.
 */
inline SharedColumnWrapper ColumnWrapper::FromArrow(const std::shared_ptr<arrow::Array> &arr) {
  auto type_id = arr->type_id();
  switch (type_id) {
    case arrow::Type::BOOL:
      return FromArrowImpl<BoolValueColumnWrapper, UDFDataType::BOOLEAN>(arr);
    case arrow::Type::INT64:
      return FromArrowImpl<Int64ValueColumnWrapper, UDFDataType::INT64>(arr);
    case arrow::Type::DOUBLE:
      return FromArrowImpl<Float64ValueColumnWrapper, UDFDataType::FLOAT64>(arr);
    case arrow::Type::STRING:
      return FromArrowImpl<StringValueColumnWrapper, UDFDataType::STRING>(arr);
    default:
      CHECK(0) << "Unknown arrow type: " << type_id;
  }
}

/**
 * Create a column wrapper.
 * @param data_type The UDFDataType
 * @param size The length of the column.
 * @return A shared_ptr to the ColumnWrapper.
 * PL_CARNOT_UPDATE_FOR_NEW_TYPES.
 */
inline SharedColumnWrapper ColumnWrapper::Make(UDFDataType data_type, size_t size) {
  switch (data_type) {
    case UDFDataType::BOOLEAN:
      return std::make_shared<BoolValueColumnWrapper>(size);
    case UDFDataType::INT64:
      return std::make_shared<Int64ValueColumnWrapper>(size);
    case UDFDataType::FLOAT64:
      return std::make_shared<Float64ValueColumnWrapper>(size);
    case UDFDataType::STRING:
      return std::make_shared<StringValueColumnWrapper>(size);
    default:
      CHECK(0) << "Unknown data type";
  }
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
