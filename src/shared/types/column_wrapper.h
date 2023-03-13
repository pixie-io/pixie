/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <arrow/array.h>
#include <arrow/buffer.h>
#include <arrow/builder.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/type_utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace types {

class ColumnWrapper;
using SharedColumnWrapper = std::shared_ptr<ColumnWrapper>;
using ColumnWrapperRecordBatch = std::vector<types::SharedColumnWrapper>;

// TODO(oazizi): Find a better place for this.
using TabletID = std::string;
using TabletIDView = std::string_view;

/**
 * Column wrapper stores underlying data so that it can be retrieved in a type erased way
 * to allow column chucks to be transparently passed.
 */
class ColumnWrapper {
 public:
  ColumnWrapper() = default;
  virtual ~ColumnWrapper() = default;

  static SharedColumnWrapper Make(DataType data_type, size_t size);
  static SharedColumnWrapper FromArrow(const std::shared_ptr<arrow::Array>& arr);
  // Force data type of resultant wrapper, regardless of the type of the underlying arrow::Array.
  // This is needed in order to create a Time64NS ColumnWrapper from an arrow Array, since the
  // underlying arrow::Array is always of type INT64.
  static SharedColumnWrapper FromArrow(DataType data_type,
                                       const std::shared_ptr<arrow::Array>& arr);

  virtual BaseValueType* UnsafeRawData() = 0;
  virtual const BaseValueType* UnsafeRawData() const = 0;
  virtual DataType data_type() const = 0;
  virtual size_t Size() const = 0;
  virtual bool Empty() const = 0;
  virtual int64_t Bytes() const = 0;

  virtual void Reserve(size_t size) = 0;
  virtual void Clear() = 0;
  virtual void ShrinkToFit() = 0;
  virtual std::shared_ptr<arrow::Array> ConvertToArrow(arrow::MemoryPool* mem_pool) = 0;
  // GetView returns an empty string view for all non-string columns.
  virtual std::string_view GetView(size_t idx) const = 0;

  template <class TValueType>
  void Append(TValueType val);

  template <class TValueType>
  TValueType& Get(size_t idx);

  template <class TValueType>
  TValueType Get(size_t idx) const;

  template <class TValueType>
  void AppendNoTypeCheck(TValueType val);

  template <class TValueType>
  TValueType& GetNoTypeCheck(size_t idx);

  template <class TValueType>
  TValueType GetNoTypeCheck(size_t idx) const;

  template <class TValueType>
  void AppendFromVector(const std::vector<TValueType>& value_vector);

  // Return a new SharedColumnWrapper with values according to the spec:
  //    { data[idx[0]], data[idx[1]], data[idx[2]], ... }
  // CopyIndexes leaves the original untouched, while MoveIndexes destroys the moved indexes.
  virtual SharedColumnWrapper CopyIndexes(const std::vector<size_t>& indexes) const = 0;
  virtual SharedColumnWrapper MoveIndexes(const std::vector<size_t>& indexes) = 0;
};

/**
 * Implementation of type erased vectors for a specific data type.
 * @tparam T The UDFValueType.
 */
template <typename T>
class ColumnWrapperTmpl : public ColumnWrapper {
 public:
  explicit ColumnWrapperTmpl(size_t size) : data_(size) {}
  explicit ColumnWrapperTmpl(size_t size, const T& val) : data_(size, val) {}
  explicit ColumnWrapperTmpl(const std::vector<T>& vals) : data_(vals) {}

  ~ColumnWrapperTmpl() override = default;

  T* UnsafeRawData() override { return data_.data(); }
  const T* UnsafeRawData() const override { return data_.data(); }
  DataType data_type() const override { return ValueTypeTraits<T>::data_type; }

  size_t Size() const override { return data_.size(); }
  bool Empty() const override { return data_.empty(); }

  std::shared_ptr<arrow::Array> ConvertToArrow(arrow::MemoryPool* mem_pool) override {
    return ToArrow(data_, mem_pool);
  }

  T operator[](size_t idx) const { return data_[idx]; }

  T& operator[](size_t idx) { return data_[idx]; }

  void Append(T val) { data_.push_back(val); }

  void Reserve(size_t size) override { data_.reserve(size); }

  void ShrinkToFit() override { data_.shrink_to_fit(); }

  void Resize(size_t size) { data_.resize(size); }

  void Clear() override { data_.clear(); }

  int64_t Bytes() const override;

  std::string_view GetView(size_t idx) const override {
    if constexpr (std::is_same_v<T, StringValue>) {
      return std::string_view(data_[idx]);
    }
    return {};
  }

  void AppendFromVector(const std::vector<T>& value_vector) {
    for (const auto& value : value_vector) {
      Append(value);
    }
  }

  // Return a new SharedColumnWrapper with values according to the spec:
  //    { data[idx[0]], data[idx[1]], data[idx[2]], ... }
  SharedColumnWrapper CopyIndexes(const std::vector<size_t>& indexes) const override {
    DCHECK_LE(indexes.size(), data_.size());
    auto copy = std::make_shared<ColumnWrapperTmpl<T>>(indexes.size());
    for (size_t i = 0; i < indexes.size(); ++i) {
      copy->data_[i] = data_[indexes[i]];
    }
    return copy;
  }

  // Return a new SharedColumnWrapper with values according to the spec:
  //    { data[idx[0]], data[idx[1]], data[idx[2]], ... }
  // Warning: Indexes in "this" ColumnWrapper have their contents moved,
  // so "this" should be discarded.
  SharedColumnWrapper MoveIndexes(const std::vector<size_t>& indexes) override {
    DCHECK_LE(indexes.size(), data_.size());
    auto col = std::make_shared<ColumnWrapperTmpl<T>>(indexes.size());
    for (size_t i = 0; i < indexes.size(); ++i) {
      col->data_[i] = std::move(data_[indexes[i]]);
    }
    return col;
  }

 private:
  std::vector<T> data_;
};

template <typename T>
int64_t ColumnWrapperTmpl<T>::Bytes() const {
  return Size() * sizeof(T);
}

template <>
inline int64_t ColumnWrapperTmpl<StringValue>::Bytes() const {
  int64_t bytes = 0;
  for (const auto& data : data_) {
    bytes += data.bytes();
  }
  return bytes;
}

// PX_CARNOT_UPDATE_FOR_NEW_TYPES.
using BoolValueColumnWrapper = ColumnWrapperTmpl<BoolValue>;
using Int64ValueColumnWrapper = ColumnWrapperTmpl<Int64Value>;
using UInt128ValueColumnWrapper = ColumnWrapperTmpl<UInt128Value>;
using Float64ValueColumnWrapper = ColumnWrapperTmpl<Float64Value>;
using StringValueColumnWrapper = ColumnWrapperTmpl<StringValue>;
using Time64NSValueColumnWrapper = ColumnWrapperTmpl<Time64NSValue>;

template <typename TColumnWrapper, types::DataType DType>
inline SharedColumnWrapper FromArrowImpl(const std::shared_ptr<arrow::Array>& arr) {
  CHECK_EQ(arr->type_id(), DataTypeTraits<DType>::arrow_type_id);
  size_t size = arr->length();
  auto wrapper = TColumnWrapper::Make(DType, size);
  auto arr_casted = static_cast<typename DataTypeTraits<DType>::arrow_array_type*>(arr.get());
  typename DataTypeTraits<DType>::value_type* out_data =
      static_cast<TColumnWrapper*>(wrapper.get())->UnsafeRawData();
  for (size_t i = 0; i < size; ++i) {
    out_data[i] = arr_casted->Value(i);
  }
  return wrapper;
}

template <>
inline SharedColumnWrapper FromArrowImpl<StringValueColumnWrapper, DataType::STRING>(
    const std::shared_ptr<arrow::Array>& arr) {
  CHECK_EQ(arr->type_id(), DataTypeTraits<types::STRING>::arrow_type_id);
  size_t size = arr->length();
  auto wrapper = StringValueColumnWrapper::Make(types::STRING, size);
  auto arr_casted = static_cast<arrow::StringArray*>(arr.get());
  StringValue* out_data = static_cast<StringValueColumnWrapper*>(wrapper.get())->UnsafeRawData();
  for (size_t i = 0; i < size; ++i) {
    out_data[i] = arr_casted->GetString(i);
  }
  return wrapper;
}

/**
 * Create a type erased ColumnWrapper from an ArrowArray.
 * @param arr the arrow array.
 * @return A shared_ptr to the ColumnWrapper.
 * PX_CARNOT_UPDATE_FOR_NEW_TYPES.
 */
inline SharedColumnWrapper ColumnWrapper::FromArrow(const std::shared_ptr<arrow::Array>& arr) {
  auto type_id = arr->type_id();
#define EXPR_CASE(_dt_) DataTypeTraits<_dt_>::arrow_type_id
#define TYPE_CASE(_dt_) \
  return FromArrowImpl<ColumnWrapperTmpl<DataTypeTraits<_dt_>::value_type>, _dt_>(arr);
  PX_SWITCH_FOREACH_DATATYPE_WITHEXPR(type_id, EXPR_CASE, TYPE_CASE);
#undef EXPR_CASE
#undef TYPE_CASE
}

inline SharedColumnWrapper ColumnWrapper::FromArrow(DataType data_type,
                                                    const std::shared_ptr<arrow::Array>& arr) {
#define TYPE_CASE(_dt_) \
  return FromArrowImpl<ColumnWrapperTmpl<DataTypeTraits<_dt_>::value_type>, _dt_>(arr);
  PX_SWITCH_FOREACH_DATATYPE(data_type, TYPE_CASE);
#undef TYPE_CASE
}

/**
 * Create a column wrapper.
 * @param data_type The UDFDataType
 * @param size The length of the column.
 * @return A shared_ptr to the ColumnWrapper.
 * PX_CARNOT_UPDATE_FOR_NEW_TYPES.
 */
inline SharedColumnWrapper ColumnWrapper::Make(DataType data_type, size_t size) {
#define TYPE_CASE(_dt_) \
  return std::make_shared<ColumnWrapperTmpl<DataTypeTraits<_dt_>::value_type>>(size);
  PX_SWITCH_FOREACH_DATATYPE(data_type, TYPE_CASE);
#undef TYPE_CASE
}

template <class TValueType>
inline void ColumnWrapper::Append(TValueType val) {
  CHECK_EQ(data_type(), ValueTypeTraits<TValueType>::data_type)
      << "Expect " << ToString(data_type()) << " got "
      << ToString(ValueTypeTraits<TValueType>::data_type);
  static_cast<ColumnWrapperTmpl<TValueType>*>(this)->Append(val);
}

template <class TValueType>
inline TValueType& ColumnWrapper::Get(size_t idx) {
  CHECK_EQ(data_type(), ValueTypeTraits<TValueType>::data_type);
  return static_cast<ColumnWrapperTmpl<TValueType>*>(this)->operator[](idx);
}

template <class TValueType>
inline TValueType ColumnWrapper::Get(size_t idx) const {
  CHECK_EQ(data_type(), ValueTypeTraits<TValueType>::data_type);
  return static_cast<const ColumnWrapperTmpl<TValueType>*>(this)->operator[](idx);
}

template <class TValueType>
inline void ColumnWrapper::AppendNoTypeCheck(TValueType val) {
  DCHECK_EQ(data_type(), ValueTypeTraits<TValueType>::data_type);
  static_cast<ColumnWrapperTmpl<TValueType>*>(this)->Append(val);
}

template <class TValueType>
inline TValueType& ColumnWrapper::GetNoTypeCheck(size_t idx) {
  DCHECK_EQ(data_type(), ValueTypeTraits<TValueType>::data_type);
  return static_cast<ColumnWrapperTmpl<TValueType>*>(this)->operator[](idx);
}

template <class TValueType>
inline TValueType ColumnWrapper::GetNoTypeCheck(size_t idx) const {
  DCHECK_EQ(data_type(), ValueTypeTraits<TValueType>::data_type);
  return static_cast<ColumnWrapperTmpl<TValueType>*>(this)->operator[](idx);
}

template <class TValueType>
inline void ColumnWrapper::AppendFromVector(const std::vector<TValueType>& val) {
  CHECK_EQ(data_type(), ValueTypeTraits<TValueType>::data_type)
      << "Expect " << ToString(data_type()) << " got "
      << ToString(ValueTypeTraits<TValueType>::data_type);
  static_cast<ColumnWrapperTmpl<TValueType>*>(this)->AppendFromVector(val);
}

template <DataType DT>
struct ColumnWrapperType {};

template <>
struct ColumnWrapperType<DataType::BOOLEAN> {
  using type = BoolValueColumnWrapper;
};

template <>
struct ColumnWrapperType<DataType::INT64> {
  using type = Int64ValueColumnWrapper;
};

template <>
struct ColumnWrapperType<DataType::UINT128> {
  using type = UInt128ValueColumnWrapper;
};

template <>
struct ColumnWrapperType<DataType::FLOAT64> {
  using type = Float64ValueColumnWrapper;
};

template <>
struct ColumnWrapperType<DataType::TIME64NS> {
  using type = Time64NSValueColumnWrapper;
};

template <>
struct ColumnWrapperType<DataType::STRING> {
  using type = StringValueColumnWrapper;
};

template <types::DataType DT>
void ExtractValueToColumnWrapper(ColumnWrapper* wrapper, arrow::Array* arr, int64_t row_idx) {
  static_cast<typename ColumnWrapperType<DT>::type*>(wrapper)->Append(
      types::GetValueFromArrowArray<DT>(arr, row_idx));
}

template <types::DataType T>
class ColumnWrapperIterator : public std::iterator<std::bidirectional_iterator_tag,
                                                   typename types::DataTypeTraits<T>::native_type> {
  using ReturnType = typename types::DataTypeTraits<T>::native_type;
  using ValueType = typename types::DataTypeTraits<T>::value_type;

 public:
  ColumnWrapperIterator(ColumnWrapper* column, int64_t idx) : column_(column), curr_idx_(idx) {}
  explicit ColumnWrapperIterator(ColumnWrapper* column) : ColumnWrapperIterator(column, 0) {}

  bool operator==(const ColumnWrapperIterator<T>& iterator) const {
    return this->column_ == iterator.column_ && this->curr_idx_ == iterator.curr_idx_;
  }

  bool operator!=(const ColumnWrapperIterator<T>& iterator) const {
    return this->column_ != iterator.column_ || this->curr_idx_ != iterator.curr_idx_;
  }

  ReturnType operator*() const {
    if constexpr (std::is_same_v<ValueType, StringValue>) {
      return column_->Get<ValueType>(curr_idx_);
    } else {
      return column_->Get<ValueType>(curr_idx_).val;
    }
  }

  ReturnType* operator->() const {
    if constexpr (std::is_same_v<ValueType, StringValue>) {
      return &column_->Get<ValueType>(curr_idx_);
    } else {
      return &column_->Get<ValueType>(curr_idx_).val;
    }
  }

  ColumnWrapperIterator<T>& operator++() {
    curr_idx_++;
    return *this;
  }
  ColumnWrapperIterator<T>& operator--() {
    curr_idx_--;
    return *this;
  }

  ColumnWrapperIterator<T> begin() { return ColumnWrapperIterator<T>(column_, 0); }

  ColumnWrapperIterator<T> end() { return ColumnWrapperIterator<T>(column_, column_->Size()); }

  ColumnWrapperIterator<T> operator++(int) {
    auto ret = *this;
    ++*this;
    return ret;
  }
  ColumnWrapperIterator<T> operator+(int i) const {
    auto ret = ColumnWrapperIterator<T>(column_, curr_idx_ + i);
    return ret;
  }
  ColumnWrapperIterator<T> operator--(int) {
    auto ret = *this;
    --*this;
    return ret;
  }
  ColumnWrapperIterator<T> operator-(int i) const {
    auto ret = ColumnWrapperIterator<T>(column_, curr_idx_ - i);
    return ret;
  }

 private:
  ColumnWrapper* column_;
  int64_t curr_idx_ = 0;
};
}  // namespace types
}  // namespace px
