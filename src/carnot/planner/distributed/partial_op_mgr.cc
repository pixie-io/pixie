#include "src/carnot/planner/distributed/partial_op_mgr.h"

#include <string>
#include <utility>
#include <vector>

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

StatusOr<OperatorIR*> AggOperatorMgr::CreatePrepareOperator(IR* plan, OperatorIR* op) const {
  DCHECK(Matches(op));
  BlockingAggIR* agg = static_cast<BlockingAggIR*>(op);
  PL_ASSIGN_OR_RETURN(BlockingAggIR * new_agg, plan->CopyNode(agg));
  PL_RETURN_IF_ERROR(new_agg->CopyParentsFrom(agg));
  new_agg->SetPartialAgg(true);
  new_agg->SetFinalizeResults(false);

  // Add the columns for the groups.
  std::vector<types::DataType> col_types;
  std::vector<std::string> col_names;
  for (ColumnIR* group : agg->groups()) {
    DCHECK(group->IsDataTypeEvaluated());
    col_types.push_back(group->EvaluatedDataType());
    col_names.push_back(group->col_name());
  }

  // Add column for the serialized expression
  col_names.push_back("serialized_expressions");
  col_types.push_back(types::STRING);
  PL_RETURN_IF_ERROR(new_agg->SetRelation(
      table_store::schema::Relation(std::move(col_types), std::move(col_names))));

  DCHECK(Match(new_agg, PartialAgg()));
  return new_agg;
}

StatusOr<OperatorIR*> AggOperatorMgr::CreateMergeOperator(IR* plan, OperatorIR* new_parent,
                                                          OperatorIR* op) const {
  DCHECK(Matches(op));
  BlockingAggIR* agg = static_cast<BlockingAggIR*>(op);
  PL_ASSIGN_OR_RETURN(BlockingAggIR * new_agg, plan->CopyNode(agg));
  PL_RETURN_IF_ERROR(new_agg->AddParent(new_parent));
  new_agg->SetPartialAgg(false);
  new_agg->SetFinalizeResults(true);
  DCHECK(Match(new_agg, FinalizeAgg()));
  return new_agg;
}
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
