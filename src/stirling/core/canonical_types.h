#pragma once

#include <map>
#include <string_view>

#include "src/stirling/core/types.h"

namespace px {
namespace stirling {
namespace canonical_data_elements {

// clang-format off

constexpr DataElement kTime = {
    "time_",
    "Timestamp when the data record was collected.",
    types::DataType::TIME64NS,
    types::SemanticType::ST_NONE,
    types::PatternType::METRIC_COUNTER};

constexpr DataElement kUPID = {
    "upid",
    "An opaque numeric ID that globally identify a running process inside the cluster.",
    types::DataType::UINT128,
    types::SemanticType::ST_UPID,
    types::PatternType::GENERAL};

// clang-format on

}  // namespace canonical_data_elements
}  // namespace stirling
}  // namespace px
