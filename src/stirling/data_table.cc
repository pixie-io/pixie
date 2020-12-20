#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/type_utils.h"
#include "src/stirling/data_table.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

using types::ColumnWrapper;
using types::DataType;

DataTable::DataTable(const DataTableSchema& schema) : table_schema_(schema) {}

void DataTable::InitBuffers(types::ColumnWrapperRecordBatch* record_batch_ptr) {
  DCHECK(record_batch_ptr != nullptr);
  DCHECK(record_batch_ptr->empty());

  for (const auto& element : table_schema_.elements()) {
    pl::types::DataType type = element.type();

#define TYPE_CASE(_dt_)                           \
  auto col = types::ColumnWrapper::Make(_dt_, 0); \
  col->Reserve(kTargetCapacity);                  \
  record_batch_ptr->push_back(col);
    PL_SWITCH_FOREACH_DATATYPE(type, TYPE_CASE);
#undef TYPE_CASE
  }
}

Tablet* DataTable::GetTablet(types::TabletIDView tablet_id) {
  auto& tablet = tablets_[tablet_id];
  if (tablet.records.empty()) {
    InitBuffers(&tablet.records);
  }
  return &tablet;
}

namespace {
// Computes a reorder vector that specifies the sorted order.
// Note 1: ColumnWrapper itself is not modified.
// Note 2: There are different ways to define the reorder indexes.
// Here we use the form where the result, idx, is used to sort x according to:
//    { x[idx[0]], x[idx[1]], x[idx[2]], ... }
std::vector<size_t> SortedIndexes(const std::vector<uint64_t>& v) {
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
class indexed_vector_iterator {
 public:
  typedef int difference_type;
  typedef T value_type;
  typedef const T& reference;
  typedef const T* pointer;
  typedef std::forward_iterator_tag iterator_category;

  indexed_vector_iterator(const std::vector<T>& data,
                          std::vector<size_t>::const_iterator index_iter)
      : data_(&data), iter_(index_iter) {}

  indexed_vector_iterator operator++() {
    indexed_vector_iterator i = *this;
    iter_++;
    return i;
  }

  indexed_vector_iterator operator++(int) {
    iter_++;
    return *this;
  }

  indexed_vector_iterator operator+(int n) {
    iter_ = iter_ + n;
    return *this;
  }

  reference operator*() { return (*data_)[*iter_]; }

  pointer operator->() { return (*data_)[*iter_]; }

  bool operator==(const indexed_vector_iterator& rhs) { return iter_ == rhs.iter_; }

  bool operator!=(const indexed_vector_iterator& rhs) { return iter_ != rhs.iter_; }

  difference_type operator-(const indexed_vector_iterator<T>& other) {
    return std::distance(other.iter_, iter_);
  }

 private:
  const std::vector<T>* data_;
  std::vector<size_t>::const_iterator iter_;
};

}  // namespace

// Searches for multiple values in a vector,
// returning the lowest positions that are greater than or equal to the search value.
// Both vectors must be sorted.
// Uses std::lower_bound, which is a binary search for efficiency.
template <size_t N>
std::array<size_t, N> SplitSortedVector(const std::vector<uint64_t>& vec,
                                        const std::vector<size_t> sort_indexes,
                                        std::array<uint64_t, N> split_vals) {
  std::array<size_t, N> out;

  auto begin = indexed_vector_iterator(vec, sort_indexes.begin());
  auto end = indexed_vector_iterator(vec, sort_indexes.end());

  auto iter = begin;
  for (size_t i = 0; i < N; ++i) {
    iter = std::lower_bound(iter, end, split_vals[i]);
    out[i] = iter - begin;
  }

  return out;
}

// Make sure the version with N=2 exists in the library.
template std::array<size_t, 2> SplitSortedVector(const std::vector<uint64_t>& vec,
                                                 const std::vector<size_t> sort_indexes,
                                                 std::array<uint64_t, 2> split_vals);

std::vector<TaggedRecordBatch> DataTable::ConsumeRecords() {
  std::vector<TaggedRecordBatch> tablets_out;
  absl::flat_hash_map<types::TabletID, Tablet> carryover_tablets;
  uint64_t next_start_time = start_time_;

  for (auto& [tablet_id, tablet] : tablets_) {
    // Sort based on times.
    // TODO(oazizi): Could keep track of whether tablet is already sorted to avoid some work.
    //               Many tables will naturally be in sorted order.
    std::vector<size_t> sort_indexes = SortedIndexes(tablet.times);

    // End time is cutoff time + 1, so call to SplitSortedVector() produces the following
    // classification: which classified according to:
    //   expired < start_time
    //   pushable <= end_time
    uint64_t end_time = cutoff_time_.has_value() ? (cutoff_time_.value() + 1)
                                                 : std::numeric_limits<uint64_t>::max();

    // Split the indexes into three groups:
    // 1) Expired indexes: these are too old to return.
    // 2) Pushable indexes: these are the ones that we return.
    // 3) Carryover indexes: these are too new to return, so hold on to them until the next round.
    auto positions = SplitSortedVector<2>(tablet.times, sort_indexes, {start_time_, end_time});
    int num_expired = positions[0];
    int num_pushable = positions[1] - positions[0];
    int num_carryover = tablet.times.size() - positions[1];

    // Case 1: Expired records. Just print a message.
    LOG_IF(WARNING, num_expired != 0) << absl::Substitute(
        "$0 records for table $1 dropped due to late arrival", num_expired, table_schema_.name());

    // Case 2: Pushable records. Copy to output.
    if (num_pushable > 0) {
      // TODO(oazizi): Consider VectorView to avoid copying.
      std::vector<size_t> push_indexes(sort_indexes.begin() + num_expired,
                                       sort_indexes.end() - num_carryover);
      types::ColumnWrapperRecordBatch pushable_records;
      for (auto& col : tablet.records) {
        pushable_records.push_back(col->MoveIndexes(push_indexes));
      }
      uint64_t last_time = tablet.times[push_indexes.back()];
      next_start_time = std::max(next_start_time, last_time);
      tablets_out.push_back(TaggedRecordBatch{tablet_id, std::move(pushable_records)});
    }

    // Case 3: Carryover records.
    if (num_carryover > 0) {
      // TODO(oazizi): Consider VectorView to avoid copying.
      std::vector<size_t> carryover_indexes(sort_indexes.begin() + num_pushable,
                                            sort_indexes.end());
      types::ColumnWrapperRecordBatch carryover_records;
      for (auto& col : tablet.records) {
        carryover_records.push_back(col->MoveIndexes(carryover_indexes));
      }

      std::vector<uint64_t> times(carryover_indexes.size());
      for (size_t i = 0; i < times.size(); ++i) {
        times[i] = tablet.times[carryover_indexes[i]];
      }
      carryover_tablets[tablet_id] =
          Tablet{tablet_id, std::move(times), std::move(carryover_records)};
    }
  }
  tablets_ = std::move(carryover_tablets);

  start_time_ = next_start_time;

  return tablets_out;
}

}  // namespace stirling
}  // namespace pl
