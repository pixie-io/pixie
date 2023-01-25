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
#include <arrow/memory_pool.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {

/*
 * Status adapter for arrow.
 */
template <>
inline Status StatusAdapter<arrow::Status>(const arrow::Status& s) noexcept {
  return Status(statuspb::UNKNOWN, s.message());
}

namespace types {

// The functions convert vector of UDF values to an arrow representation on
// the given MemoryPool.
template <typename TUDFValue>
inline std::shared_ptr<arrow::Array> ToArrow(const std::vector<TUDFValue>& data,
                                             arrow::MemoryPool* mem_pool) {
  DCHECK(mem_pool != nullptr);

  typename ValueTypeTraits<TUDFValue>::arrow_builder_type builder(mem_pool);
  PX_CHECK_OK(builder.Reserve(data.size()));
  for (const auto& v : data) {
    builder.UnsafeAppend(v.val);
  }
  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder.Finish(&arr));
  return arr;
}

// Specialization of the above for time64
template <>
inline std::shared_ptr<arrow::Array> ToArrow<Time64NSValue>(const std::vector<Time64NSValue>& data,
                                                            arrow::MemoryPool* mem_pool) {
  DCHECK(mem_pool != nullptr);

  arrow::Time64Builder builder(arrow::time64(arrow::TimeUnit::NANO), mem_pool);
  PX_CHECK_OK(builder.Reserve(data.size()));
  for (const auto& v : data) {
    builder.UnsafeAppend(v.val);
  }
  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder.Finish(&arr));
  return arr;
}

// Specialization of the above for strings.
template <>
inline std::shared_ptr<arrow::Array> ToArrow<StringValue>(const std::vector<StringValue>& data,
                                                          arrow::MemoryPool* mem_pool) {
  DCHECK(mem_pool != nullptr);
  arrow::StringBuilder builder(mem_pool);
  size_t total_size =
      std::accumulate(data.begin(), data.end(), 0ULL,
                      [](uint64_t sum, const std::string& str) { return sum + str.size(); });
  // This allocates space for null/ptrs/size.
  PX_CHECK_OK(builder.Reserve(data.size()));
  // This allocates space for the actual data.
  PX_CHECK_OK(builder.ReserveData(total_size));
  for (const auto& val : data) {
    builder.UnsafeAppend(val);
  }
  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder.Finish(&arr));
  return arr;
}

/**
 * Find the UDFDataType for a given arrow type.
 * @param arrow_type The arrow type.
 * @return The UDFDataType.
 */
DataType ArrowToDataType(const arrow::Type::type& arrow_type);

arrow::Type::type ToArrowType(const DataType& udf_type);

int64_t ArrowTypeToBytes(const arrow::Type::type& arrow_type);

/**
 * Make an arrow builder based on UDFDataType and usng the passed in MemoryPool.
 * @param data_type The UDFDataType.
 * @param mem_pool The MemoryPool to use.
 * @return a unique_ptr to an array builder.
 */
std::unique_ptr<arrow::ArrayBuilder> MakeArrowBuilder(const DataType& data_type,
                                                      arrow::MemoryPool* mem_pool);

// The get value functions pluck out value at a specific index.
template <typename T>
inline auto GetValue(const T* arr, int64_t idx) {
  return arr->Value(idx);
}

// Specialization for string type.
template <>
inline auto GetValue<arrow::StringArray>(const arrow::StringArray* arr, int64_t idx) {
  return arr->GetString(idx);
}

// This function takes in a generic arrow::Array and then converts it to actual
// specific arrow::Array subtype. This function is unsafe and will produce wrong results (or crash)
// if used incorrectly.
template <types::DataType TExecArgType>
constexpr auto GetValueFromArrowArray(const arrow::Array* arg, int64_t idx) {
  // A sample transformation (for TExecArgType = types::DataType::INT64) is:
  // return GetValue(static_cast<arrow::Int64Array*>(arg), idx);
  using arrow_array_type = typename types::DataTypeTraits<TExecArgType>::arrow_array_type;
  return GetValue(static_cast<const arrow_array_type*>(arg), idx);
}

inline std::string_view GetStringViewFromArrowArray(const arrow::Array* arr, int64_t idx) {
  DCHECK(arr->type_id() == arrow::Type::STRING);
  auto arrow_string_view = static_cast<const arrow::StringArray*>(arr)->GetView(idx);
  return std::string_view(arrow_string_view.data(), arrow_string_view.size());
}

template <types::DataType TDataType>
inline int64_t GetArrowArrayBytes(const arrow::Array* arr) {
  return arr->length() * types::ArrowTypeToBytes(types::ToArrowType(TDataType));
}

template <>
inline int64_t GetArrowArrayBytes<types::DataType::STRING>(const arrow::Array* arr) {
  int64_t total_bytes = 0;
  // Loop through each string in the Arrow array.
  for (int64_t i = 0; i < arr->length(); i++) {
    total_bytes += sizeof(char) * GetValueFromArrowArray<types::DataType::STRING>(arr, i).length();
  }
  return total_bytes;
}

template <types::DataType T>
class ArrowArrayIterator
    : public std::iterator<std::bidirectional_iterator_tag,
                           typename types::ValueTypeTraits<
                               typename types::DataTypeTraits<T>::value_type>::native_type> {
  using ReturnType =
      typename types::ValueTypeTraits<typename types::DataTypeTraits<T>::value_type>::native_type;

 public:
  ArrowArrayIterator();

  explicit ArrowArrayIterator(arrow::Array* array) : array_(array) {}

  ArrowArrayIterator(arrow::Array* array, int64_t idx) : array_(array), curr_idx_(idx) {}

  bool operator==(const ArrowArrayIterator<T>& iterator) const {
    return this->array_ == iterator.array_ && this->curr_idx_ == iterator.curr_idx_;
  }

  bool operator!=(const ArrowArrayIterator<T>& iterator) const {
    return this->array_ != iterator.array_ || this->curr_idx_ != iterator.curr_idx_;
  }

  ReturnType operator*() const { return (types::GetValueFromArrowArray<T>(array_, curr_idx_)); }

  ReturnType* operator->() const { return (types::GetValueFromArrowArray<T>(array_, curr_idx_)); }

  ArrowArrayIterator<T>& operator++() {
    curr_idx_++;

    return *this;
  }
  ArrowArrayIterator<T>& operator--() {
    curr_idx_--;
    return *this;
  }

  ArrowArrayIterator<T> begin() { return ArrowArrayIterator<T>(array_, 0); }

  ArrowArrayIterator<T> end() { return ArrowArrayIterator<T>(array_, array_->length()); }

  ArrowArrayIterator<T> operator++(int) {
    auto ret = *this;
    ++*this;
    return ret;
  }
  ArrowArrayIterator<T> operator+(int i) const {
    auto ret = ArrowArrayIterator<T>(array_, curr_idx_ + i);
    return ret;
  }
  ArrowArrayIterator<T> operator--(int) {
    auto ret = *this;
    --*this;
    return ret;
  }
  ArrowArrayIterator<T> operator-(int i) const {
    auto ret = ArrowArrayIterator<T>(array_, curr_idx_ - i);
    return ret;
  }

 private:
  arrow::Array* array_;
  int64_t curr_idx_ = 0;
};

/**
 * Search through the arrow array for the index of the first item equal or greater than the given
 * value.
 * @tparam T UDF datatype of the arrow array.
 * @param arr the arrow array to search through.
 * @param val the value to search for in the arrow array.
 * @return the index of the first item in the array equal to or greater than val.
 */
template <types::DataType T>
int64_t SearchArrowArrayGreaterThanOrEqual(
    arrow::Array* arr,
    typename types::ValueTypeTraits<typename types::DataTypeTraits<T>::value_type>::native_type
        val) {
  auto arr_iterator = ArrowArrayIterator<T>(arr);
  auto res = std::lower_bound(arr_iterator, arr_iterator.end(), val);
  if (res != arr_iterator.end()) {
    return std::distance(arr_iterator.begin(), res);
  }
  return -1;
}

/**
 * Search through the arrow array for the index of the first item less than the given value.
 * @tparam T UDF datatype of the arrow array.
 * @param arr the arrow array to search through.
 * @param val the value to search for in the arrow array.
 * @return the index of the first item in the array less than val.
 */
template <types::DataType T>
int64_t SearchArrowArrayLessThan(
    arrow::Array* arr,
    typename types::ValueTypeTraits<typename types::DataTypeTraits<T>::value_type>::native_type
        val) {
  auto res = SearchArrowArrayGreaterThanOrEqual<T>(arr, val);
  if (res == -1) {
    // Everything in the array is less than val.
    return arr->length();
  }
  if (res == 0) {
    // Nothing in the array is less than val.
    return -1;
  }
  // res points to an index that is geq than val. res - 1 should be the largest item less than
  // val. However, arr[res-1] may be a duplicate value, so we need to find the first instance of
  // arr[res-1] in the array.
  auto next_smallest = types::GetValueFromArrowArray<T>(arr, res - 1);
  return SearchArrowArrayGreaterThanOrEqual<T>(arr, next_smallest);
}

/**
 * Search through the arrow array for the index of the last item less than or equal to the given
 * value.
 */
template <types::DataType T>
int64_t SearchArrowArrayLessThanOrEqual(
    arrow::Array* arr,
    typename types::ValueTypeTraits<typename types::DataTypeTraits<T>::value_type>::native_type
        val) {
  auto arr_iterator = ArrowArrayIterator<T>(arr);
  auto res = std::upper_bound(arr_iterator, arr_iterator.end(), val);
  if (res == arr_iterator.begin()) {
    // All elements are greater than val.
    return -1;
  }
  // The element before the first greater than element is the last less than or equal element.
  res--;
  return std::distance(arr_iterator.begin(), res);
}

class TypeErasedArrowBuilder {
 public:
  virtual ~TypeErasedArrowBuilder() = default;

  virtual Status Reserve(size_t num_rows) = 0;
  virtual Status ReserveData(size_t num_bytes) = 0;
  virtual Status Finish(std::shared_ptr<arrow::Array>* out) = 0;
};

template <types::DataType TDataType>
class TypeErasedArrowBuilderImpl : public TypeErasedArrowBuilder {
  using BuilderType = typename types::DataTypeTraits<TDataType>::arrow_builder_type;

 public:
  explicit TypeErasedArrowBuilderImpl(std::unique_ptr<arrow::ArrayBuilder> builder)
      : builder_(std::move(builder)), typed_builder_(static_cast<BuilderType*>(builder_.get())) {}

  Status Reserve(size_t num_rows) override {
    PX_RETURN_IF_ERROR(typed_builder_->Reserve(num_rows));
    return Status::OK();
  }

  Status ReserveData(size_t num_bytes) override {
    if constexpr (TDataType == types::DataType::STRING) {
      PX_RETURN_IF_ERROR(typed_builder_->ReserveData(num_bytes));
    }
    return Status::OK();
  }

  Status Finish(std::shared_ptr<arrow::Array>* out) override {
    PX_RETURN_IF_ERROR(typed_builder_->Finish(out));
    return Status::OK();
  }

  template <typename TIter>
  void UnsafeAppendValues(TIter begin, TIter end) {
    for (auto it = begin; it != end; ++it) {
      typed_builder_->UnsafeAppend(*it);
    }
  }

 private:
  std::unique_ptr<arrow::ArrayBuilder> builder_;
  BuilderType* typed_builder_;
};

template <types::DataType T>
typename std::enable_if<
    arrow::TypeTraits<typename DataTypeTraits<T>::arrow_type>::is_parameter_free,
    std::unique_ptr<arrow::ArrayBuilder>>::type
GetArrowBuilder(arrow::MemoryPool* mem_pool) {
  return std::make_unique<typename DataTypeTraits<T>::arrow_builder_type>(mem_pool);
}

template <types::DataType T>
typename std::enable_if<
    !arrow::TypeTraits<typename DataTypeTraits<T>::arrow_type>::is_parameter_free,
    std::unique_ptr<arrow::ArrayBuilder>>::type
GetArrowBuilder(arrow::MemoryPool* mem_pool) {
  return std::make_unique<typename DataTypeTraits<T>::arrow_builder_type>(
      DataTypeTraits<T>::default_value(), mem_pool);
}

template <types::DataType TDataType>
TypeErasedArrowBuilderImpl<TDataType>* GetTypedArrowBuilder(TypeErasedArrowBuilder* builder) {
  return static_cast<TypeErasedArrowBuilderImpl<TDataType>*>(builder);
}

/**
 * Make a type erased arrow builder based on UDFDataType and using the passed in MemoryPool.
 * @param data_type The UDFDataType.
 * @param mem_pool The MemoryPool to use.
 * @return a unique_ptr to a type erased array builder.
 */
std::unique_ptr<TypeErasedArrowBuilder> MakeTypeErasedArrowBuilder(const DataType& data_type,
                                                                   arrow::MemoryPool* mem_pool);

}  // namespace types
}  // namespace px
