#pragma once

#include <string>

namespace px {
namespace carnot {
namespace plan {

/*
 * A PlanNode represents any object that can be a node in the execution graph.
 */
class PlanNode {
 public:
  virtual ~PlanNode() = default;

  virtual std::string DebugString() const = 0;
};

}  // namespace plan
}  // namespace carnot
}  // namespace px
