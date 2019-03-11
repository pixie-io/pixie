#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {
class IROptimizer {
 public:
  IROptimizer() = default;
  Status Optimize(IR* ir);

 private:
  Status CollapseRange(IR* ir);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
