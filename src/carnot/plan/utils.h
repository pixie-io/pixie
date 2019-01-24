#pragma once
#include <string>

#include "src/carnot/proto/plan.pb.h"
#include "src/carnot/proto/types.pb.h"

namespace pl {
namespace carnot {
namespace plan {

/**
 * ToString converts the operator enum to a string.
 */
std::string ToString(carnotpb::OperatorType op);

/**
 * ToString converts the datatype enum to a string.
 */
std::string ToString(carnotpb::DataType dt);

}  // namespace plan
}  // namespace carnot
}  // namespace pl
