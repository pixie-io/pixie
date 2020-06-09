#include "src/carnot/planner/distributed/partial_op_mgr.h"
namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<OperatorIR*> LimitOperatorMgr::CreatePrepareOperator(IR* plan, OperatorIR* op) const {
  DCHECK(Matches(op));
  LimitIR* limit = static_cast<LimitIR*>(op);
  PL_ASSIGN_OR_RETURN(LimitIR * new_limit, plan->CopyNode(limit));
  PL_RETURN_IF_ERROR(new_limit->CopyParentsFrom(limit));
  return new_limit;
}
StatusOr<OperatorIR*> LimitOperatorMgr::CreateMergeOperator(IR* plan, OperatorIR* new_parent,
                                                            OperatorIR* op) const {
  DCHECK(Matches(op));
  LimitIR* limit = static_cast<LimitIR*>(op);
  PL_ASSIGN_OR_RETURN(LimitIR * new_limit, plan->CopyNode(limit));
  PL_RETURN_IF_ERROR(new_limit->AddParent(new_parent));
  return new_limit;
}
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
