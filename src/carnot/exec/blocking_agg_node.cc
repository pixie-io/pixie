#include "src/carnot/exec/blocking_agg_node.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/plan/utils.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
namespace exec {

using SharedArray = std::shared_ptr<arrow::Array>;

std::string BlockingAggNode::DebugStringImpl() {
  // TODO(zasgar): implement.
  return "";
}

Status BlockingAggNode::InitImpl(const plan::Operator &plan_node,
                                 const RowDescriptor &output_descriptor,
                                 const std::vector<RowDescriptor> &input_descriptors) {
  CHECK(plan_node.op_type() == carnotpb::OperatorType::BLOCKING_AGGREGATE_OPERATOR);
  const auto *agg_plan_node = static_cast<const plan::BlockingAggregateOperator *>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::BlockingAggregateOperator>(*agg_plan_node);
  output_descriptor_ = std::make_unique<RowDescriptor>(output_descriptor);

  if (input_descriptors.size() != 1) {
    return error::InvalidArgument("Aggregate operator expects a single input relation, got $0",
                                  input_descriptors.size());
  }
  input_descriptor_ = std::make_unique<RowDescriptor>(input_descriptors[0]);

  for (const auto &value : plan_node_->values()) {
    if (value->ExpressionType() != plan::Expression::kAgg) {
      return error::InvalidArgument("Aggregate operator can only use aggregate expressions");
    }
  }

  size_t output_size = plan_node_->values().size() + plan_node_->groups().size();
  if (output_size != output_descriptor.size()) {
    return error::InvalidArgument("Output size mismatch in aggregate");
  }

  return Status::OK();
}

Status BlockingAggNode::PrepareImpl(ExecState *) { return Status::OK(); }

Status BlockingAggNode::OpenImpl(ExecState *exec_state) {
  if (HasNoGroups()) {
    // Find the correct UDA in registry and store the definition and instance.
    auto *uda_registry = exec_state->uda_registry();
    for (const auto &value : plan_node_->values()) {
      std::vector<udf::UDFDataType> types;
      types.reserve(value->Deps().size());
      for (auto *dep : value->Deps()) {
        PL_ASSIGN_OR_RETURN(auto type, GetTypeOfDep(*dep));
        types.push_back(type);
      }
      PL_ASSIGN_OR_RETURN(auto def, uda_registry->GetDefinition(value->name(), types));
      udas_no_groups_.emplace_back(def->Make(), def);
    }
  }
  return Status::OK();
}

Status BlockingAggNode::CloseImpl(ExecState *) {
  udas_no_groups_.clear();
  return Status::OK();
}

Status BlockingAggNode::ConsumeNextImpl(ExecState *exec_state, const RowBatch &rb) {
  if (HasNoGroups()) {
    auto values = plan_node_->values();
    for (size_t i = 0; i < values.size(); ++i) {
      PL_RETURN_IF_ERROR(
          EvaluateSingleExpressionNoGroups(exec_state, udas_no_groups_[i], values[i].get(), rb));
    }

    if (rb.eos()) {
      RowBatch output_rb(*output_descriptor_, 1);
      for (size_t i = 0; i < values.size(); ++i) {
        const auto &uda_info = udas_no_groups_[i];
        auto builder = udf::MakeArrowBuilder(uda_info.def->finalize_return_type(),
                                             exec_state->exec_mem_pool());
        PL_RETURN_IF_ERROR(
            uda_info.def->FinalizeArrow(uda_info.uda.get(), nullptr /*ctx*/, builder.get()));
        SharedArray out_col;
        PL_RETURN_IF_ERROR(builder->Finish(&out_col));
        PL_RETURN_IF_ERROR(output_rb.AddColumn(out_col));
      }
      output_rb.set_eos(true);
      PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, output_rb));
    }
  }
  return Status::OK();
}
StatusOr<udf::UDFDataType> BlockingAggNode::GetTypeOfDep(const plan::ScalarExpression &expr) const {
  // Agg exprs can only be of type col, or  const.
  switch (expr.ExpressionType()) {
    case plan::Expression::kColumn: {
      auto idx = static_cast<const plan::Column *>(&expr)->Index();
      return input_descriptor_->type(idx);
    }
    case plan::Expression::kConstant:
      return static_cast<const plan::ScalarValue *>(&expr)->DataType();
    default:
      return error::InvalidArgument("Invalid expression type in agg: $1",
                                    ToString(expr.ExpressionType()));
  }
}
Status BlockingAggNode::EvaluateSingleExpressionNoGroups(ExecState *exec_state,
                                                         const UDAInfo &uda_info,
                                                         plan::AggregateExpression *expr,
                                                         const RowBatch &input_rb) {
  plan::ExpressionWalker<StatusOr<SharedArray>> walker;
  walker.OnScalarValue(
      [&](const plan::ScalarValue &val,
          const std::vector<StatusOr<SharedArray>> &children) -> std::shared_ptr<arrow::Array> {
        DCHECK_EQ(children.size(), 0ULL);
        return EvalScalarToArrow(exec_state, val, 1);
      });

  walker.OnColumn(
      [&](const plan::Column &col,
          const std::vector<StatusOr<SharedArray>> &children) -> std::shared_ptr<arrow::Array> {
        DCHECK_EQ(children.size(), 0ULL);
        return input_rb.ColumnAt(col.Index());
      });

  walker.OnAggregateExpression(
      [&](const plan::AggregateExpression &agg,
          const std::vector<StatusOr<SharedArray>> &children) -> StatusOr<SharedArray> {
        DCHECK(agg.name() == uda_info.def->name());
        DCHECK(children.size() == uda_info.def->update_arguments().size());
        // collect the arguments.
        std::vector<const arrow::Array *> raw_children;
        raw_children.reserve(children.size());
        for (const auto &child : children) {
          if (!child.ok()) {
            return child;
          }
          raw_children.push_back(child.ValueOrDie().get());
        }
        PL_RETURN_IF_ERROR(uda_info.def->ExecBatchUpdateArrow(uda_info.uda.get(), nullptr /* ctx */,
                                                              raw_children));
        // Blocking aggregates don't produce results until all data is seen.
        return {};
      });

  PL_RETURN_IF_ERROR(walker.Walk(*expr));
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
