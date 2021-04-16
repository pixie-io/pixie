#pragma once
#include <string>
#include <vector>

#include "src/carnot/planner/ir/ir_nodes.h"

namespace px {
namespace carnot {
namespace planner {

constexpr char kAbsTimeFormat[] = "%E4Y-%m-%d %H:%M:%E*S %z";
StatusOr<int64_t> ParseDurationFmt(const StringIR* node, int64_t time_now);

StatusOr<int64_t> ParseAbsFmt(const StringIR* node, const std::string& format);

StatusOr<int64_t> ParseStringToTime(const StringIR* node, int64_t time_now);

}  // namespace planner
}  // namespace carnot
}  // namespace px
