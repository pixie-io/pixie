#pragma once

#include <vector>

namespace pl {
namespace stirling {

/**
 * Struct that should be the return type of ParseFrames() API in protocol pipeline stitchers.
 * @tparam TRecord Record type of the protocol.
 */
template <typename TRecord>
struct RecordsWithErrorCount {
  std::vector<TRecord> records;
  int error_count = 0;
};

}  // namespace stirling
}  // namespace pl
