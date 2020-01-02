#pragma once

#include <gmock/gmock.h>

#include <vector>

namespace pl {
namespace stirling {
namespace testing {

MATCHER_P(ColWrapperSizeIs, size, "") { return arg->Size() == static_cast<size_t>(size); }

std::vector<size_t> FindRecordIdxMatchesPid(const types::ColumnWrapperRecordBatch& http_record,
                                            int pid) {
  std::vector<size_t> res;
  for (size_t i = 0; i < http_record[kHTTPUPIDIdx]->Size(); ++i) {
    md::UPID upid(http_record[kHTTPUPIDIdx]->Get<types::UInt128Value>(i).val);
    if (upid.pid() == static_cast<uint64_t>(pid)) {
      res.push_back(i);
    }
  }
  return res;
}

}  // namespace testing
}  // namespace stirling
}  // namespace pl
