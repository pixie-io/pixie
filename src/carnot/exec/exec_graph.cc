#include <memory>
#include <unordered_map>

#include "src/carnot/exec/blocking_agg_node.h"
#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/map_node.h"
#include "src/carnot/exec/memory_sink_node.h"
#include "src/carnot/exec/memory_source_node.h"
#include "src/carnot/plan/compiler_state.h"
#include "src/carnot/plan/relation.h"
#include "src/carnot/plan/schema.h"
#include "src/common/object_pool.h"

namespace pl {
namespace carnot {
namespace exec {

Status ExecutionGraph::Init(std::shared_ptr<plan::Schema> schema,
                            std::shared_ptr<plan::CompilerState> compiler_state,
                            std::shared_ptr<plan::PlanFragment> pf) {
  compiler_state_ = compiler_state;
  schema_ = schema;
  pf_ = pf;

  std::unordered_map<int64_t, ExecNode*> nodes;
  std::unordered_map<int64_t, RowDescriptor> descriptors;
  plan::PlanFragmentWalker()
      .OnMap([&](auto& node) {
        return OnOperatorImpl<plan::MapOperator, MapNode>(node, &descriptors);
      })
      .OnMemorySink([&](auto& node) {
        return OnOperatorImpl<plan::MemorySinkOperator, MemorySinkNode>(node, &descriptors);
      })
      .OnBlockingAggregate([&](auto& node) {
        return OnOperatorImpl<plan::BlockingAggregateOperator, BlockingAggNode>(node, &descriptors);
      })
      .OnMemorySource([&](auto& node) {
        sources_.push_back(node.id());
        return OnOperatorImpl<plan::MemorySourceOperator, MemorySourceNode>(node, &descriptors);
      })
      .Walk(pf.get());
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
