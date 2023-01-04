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

#include <algorithm>
#include <array>
#include <vector>

namespace px {
namespace stirling {
namespace utils {

// Computes a reorder vector that specifies the sorted order.
// Note 1: ColumnWrapper itself is not modified.
// Note 2: There are different ways to define the reorder indexes.
// Here we use the form where the result, idx, is used to sort x according to:
//    { x[idx[0]], x[idx[1]], x[idx[2]], ... }
template <typename T>
std::vector<size_t> SortedIndexes(const std::vector<T>& v) {
  // Create indices corresponding to v.
  std::vector<size_t> idx(v.size());
  // Initialize idx = {0, 1, 2, 3, ... }
  for (size_t i = 0; i < idx.size(); ++i) {
    idx[i] = i;
  }

  // Find the sorted indices by running a sort on idx, but using the values of v.
  // Use std::stable_sort instead of std::sort to minimize churn in indices.
  std::stable_sort(idx.begin(), idx.end(), [&v](size_t i1, size_t i2) { return v[i1] < v[i2]; });

  return idx;
}

// An iterator that walks over a vector according to provided indexes.
// Used in conjunction with SortedIndexes to iterate through an unsorted vector in sorted order.
template <typename T>
class IndexedVectorIterator {
 public:
  using difference_type = int;
  using value_type = T;
  using reference = const T&;
  using pointer = const T*;
  using iterator_category = std::random_access_iterator_tag;

  IndexedVectorIterator(const std::vector<T>& data, std::vector<size_t>::const_iterator index_iter)
      : data_(&data), iter_(index_iter) {}

  IndexedVectorIterator operator++() {
    ++iter_;
    return *this;
  }

  IndexedVectorIterator operator++(int) {
    IndexedVectorIterator i = *this;
    ++iter_;
    return i;
  }

  IndexedVectorIterator operator+(int n) { return IndexedVectorIterator(data_, iter_ + n); }

  IndexedVectorIterator operator+=(int n) {
    iter_ += n;
    return *this;
  }

  IndexedVectorIterator operator--() {
    --iter_;
    return *this;
  }

  IndexedVectorIterator operator--(int) {
    IndexedVectorIterator i = *this;
    --iter_;
    return i;
  }

  IndexedVectorIterator operator-(int n) { return IndexedVectorIterator(data_, iter_ - n); }

  IndexedVectorIterator operator-=(int n) {
    iter_ -= n;
    return *this;
  }

  reference operator*() { return (*data_)[*iter_]; }

  pointer operator->() { return (*data_)[*iter_]; }

  bool operator==(const IndexedVectorIterator& rhs) { return iter_ == rhs.iter_; }

  bool operator!=(const IndexedVectorIterator& rhs) { return iter_ != rhs.iter_; }

  difference_type operator-(const IndexedVectorIterator<T>& other) {
    return std::distance(other.iter_, iter_);
  }

 private:
  const std::vector<T>* data_;
  std::vector<size_t>::const_iterator iter_;
};

// Searches for multiple values in a vector,
// returning the lowest positions that are greater than or equal to the search value.
// Uses std::lower_bound, which is a binary search for efficiency.
template <size_t N, typename T>
std::array<size_t, N> SplitSortedVector(const std::vector<T>& vec,
                                        const std::vector<size_t>& sort_indexes,
                                        std::array<T, N> split_vals) {
  std::array<size_t, N> out;

  auto begin = IndexedVectorIterator(vec, sort_indexes.begin());
  auto end = IndexedVectorIterator(vec, sort_indexes.end());

  auto iter = begin;
  for (size_t i = 0; i < N; ++i) {
    iter = std::lower_bound(iter, end, split_vals[i]);
    out[i] = iter - begin;
  }

  return out;
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
