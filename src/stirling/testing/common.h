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

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/common/system/proc_parser.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/output.h"
#include "src/stirling/testing/overloads.h"

#define ASSERT_NOT_EMPTY_AND_GET_RECORDS(lhs, tablets) \
  ASSERT_EQ(tablets.size(), 1);                        \
  lhs = tablets[0].records;

using ::testing::IsSupersetOf;

namespace px {

namespace types {

// Teach gMock to print out a SharedColumnWrapper.
inline std::ostream& operator<<(std::ostream& os, const ::px::types::SharedColumnWrapper& col) {
  os << absl::Substitute("size=$0", col->Size());
  return os;
}

}  // namespace types

namespace stirling {
namespace testing {

MATCHER_P(ColWrapperSizeIs, size, absl::Substitute("is a ColumnWrapper having $0 elements", size)) {
  return arg->Size() == static_cast<size_t>(size);
}

MATCHER_P(RecordBatchSizeIs, size, absl::Substitute("is a RecordBatch having $0 elements", size)) {
  // Check consistency of record back. All columns should have the same size.
  for (const auto& col : arg) {
    CHECK_EQ(col->Size(), arg[0]->Size());
  }

  return !arg.empty() && arg[0]->Size() == static_cast<size_t>(size);
}

MATCHER(ColWrapperIsEmpty, "is an empty ColumnWrapper") { return arg->Empty(); }

inline std::vector<size_t> FindRecordIdxMatchesPIDs(const types::ColumnWrapperRecordBatch& record,
                                                    int upid_column_idx,
                                                    const std::vector<int>& pids) {
  std::vector<size_t> res;

  for (size_t i = 0; i < record[upid_column_idx]->Size(); ++i) {
    md::UPID upid(record[upid_column_idx]->Get<types::UInt128Value>(i).val);
    for (const int pid : pids) {
      if (upid.pid() == static_cast<uint64_t>(pid)) {
        res.push_back(i);
      }
    }
  }
  return res;
}

inline std::vector<size_t> FindRecordIdxMatchesPID(const types::ColumnWrapperRecordBatch& record,
                                                   int upid_column_idx, int pid) {
  return FindRecordIdxMatchesPIDs(record, upid_column_idx, {pid});
}

inline std::shared_ptr<types::ColumnWrapper> SelectColumnWrapperRows(
    const types::ColumnWrapper& src, const std::vector<size_t>& indices) {
  auto out = types::ColumnWrapper::Make(src.data_type(), 0);
  out->Reserve(indices.size());

#define TYPE_CASE(_dt_)                                                 \
  for (int idx : indices) {                                             \
    out->Append(src.Get<types::DataTypeTraits<_dt_>::value_type>(idx)); \
  }
  PX_SWITCH_FOREACH_DATATYPE(src.data_type(), TYPE_CASE);
#undef TYPE_CASE

  return out;
}

inline types::ColumnWrapperRecordBatch SelectRecordBatchRows(
    const types::ColumnWrapperRecordBatch& src, const std::vector<size_t>& indices) {
  types::ColumnWrapperRecordBatch out;
  for (const auto& col : src) {
    out.push_back(SelectColumnWrapperRows(*col, indices));
  }

  return out;
}

inline types::ColumnWrapperRecordBatch FindRecordsMatchingPID(
    const types::ColumnWrapperRecordBatch& rb, int upid_column_idx, int pid) {
  std::vector<size_t> indices = FindRecordIdxMatchesPID(rb, upid_column_idx, pid);
  return SelectRecordBatchRows(rb, indices);
}

// Note the index is column major, so it comes before row_idx.
template <typename TValueType>
inline const TValueType& AccessRecordBatch(const types::ColumnWrapperRecordBatch& record_batch,
                                           int column_idx, int row_idx) {
  return record_batch[column_idx]->Get<TValueType>(row_idx);
}

template <>
inline const std::string& AccessRecordBatch<std::string>(
    const types::ColumnWrapperRecordBatch& record_batch, int column_idx, int row_idx) {
  return record_batch[column_idx]->Get<types::StringValue>(row_idx);
}

inline md::UPID PIDToUPID(pid_t pid) {
  system::ProcParser proc_parser;
  return md::UPID{/*asid*/ 0, static_cast<uint32_t>(pid),
                  proc_parser.GetPIDStartTimeTicks(pid).ValueOrDie()};
}

// Returns a list of string representation of the records in the input data table.
// The records in the data table is removed afterwards.
inline std::string ExtractToString(const DataTableSchema& data_table_schema,
                                   DataTable* data_table) {
  std::vector<std::string> res;
  std::vector<TaggedRecordBatch> tagged_record_batches = data_table->ConsumeRecords();
  for (auto& tagged_record_batch : tagged_record_batches) {
    types::ColumnWrapperRecordBatch batches{std::move(tagged_record_batch.records)};
    auto lines = ToString(data_table_schema.ToProto(), batches);
    res.insert(res.end(), std::make_move_iterator(lines.begin()),
               std::make_move_iterator(lines.end()));
  }
  return absl::StrJoin(res, "\n");
}

// Returns a list of string representation of the records in the input data table.
// The records in the data table is removed afterwards.
inline types::ColumnWrapperRecordBatch ExtractRecordsMatchingPID(DataTable* data_table,
                                                                 int upid_column_idx, int pid) {
  types::ColumnWrapperRecordBatch res;
  std::vector<TaggedRecordBatch> tagged_record_batches = data_table->ConsumeRecords();
  for (auto& tagged_record_batch : tagged_record_batches) {
    types::ColumnWrapperRecordBatch batches{std::move(tagged_record_batch.records)};
    for (auto& record : FindRecordsMatchingPID(batches, upid_column_idx, pid)) {
      res.push_back(std::move(record));
    }
  }
  return res;
}

class Timeout {
 public:
  explicit Timeout(std::chrono::nanoseconds timeout = std::chrono::minutes{5})
      : timeout_(timeout), start_(std::chrono::steady_clock::now()) {}

  bool TimedOut() { return !((std::chrono::steady_clock::now() - start_) < timeout_); }

 private:
  std::chrono::nanoseconds timeout_;
  std::chrono::time_point<std::chrono::steady_clock> start_;
};

template <typename TRecord>
bool RecordsContains(const std::vector<TRecord>& records, const std::vector<TRecord>& expected) {
  for (const auto& expected_record : expected) {
    bool in_records = false;
    for (const auto& r : records) {
      if (expected_record == r) {
        in_records = true;
        break;
      }
    }
    if (!in_records) {
      return false;
    }
  }
  return true;
}

// WaitAndExpectRecords calls `get_records` repeatedly until all of the `expected` records are
// contained in the response to `get_records`. `TRecord` is required to have a operator== overload,
// and suggested to have a PrintTo overload, see overloads.h for examples. `TGetRecords`
// should be a function that returns a std::vector<TRecord>.
template <typename TRecord, typename TGetRecords>
std::vector<TRecord> WaitAndExpectRecords(
    TGetRecords get_records, std::vector<TRecord> expected,
    std::chrono::nanoseconds sleep_time = std::chrono::milliseconds{200}) {
  std::vector<TRecord> records;
  Timeout t;
  while (!RecordsContains(records, expected) && !t.TimedOut()) {
    records = get_records();
    std::this_thread::sleep_for(sleep_time);
  }
  EXPECT_THAT(records, IsSupersetOf(expected));
  return records;
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
