#include "src/carnot/compiler/ir_relation_handler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace compiler {
IRRelationHandler::IRRelationHandler(CompilerState* compiler_state) {
  compiler_state_ = compiler_state;
}
/**
 * @brief Iterates through all of the IR columns and makes sure
 * that they are read to be transposed into the logical plan nodes.
 *
 * @param ir_graph
 * @return Status
 */
std::vector<Status> IRRelationHandler::VerifyIRColumnsReady(IR* ir_graph) {
  std::vector<Status> exprs;
  for (auto& i : ir_graph->dag().TopologicalSort()) {
    auto node = ir_graph->Get(i);
    if (node->type() == ColumnType) {
      auto col_node = static_cast<ColumnIR*>(node);
      if (!col_node->col_idx_set()) {
        exprs.push_back(
            error::InvalidArgument("ColNode(id=$0) has not been validated.", col_node->id()));
      }
    }
  }
  return exprs;
}

Status IRRelationHandler::HasExpectedColumns(
    const std::unordered_set<std::string>& expected_columns,
    const table_store::schema::Relation& parent_relation) {
  std::vector<std::string> missing_cols;
  for (auto& c : expected_columns) {
    if (!parent_relation.HasColumn(c)) {
      missing_cols.push_back(c);
    }
  }
  if (!missing_cols.empty()) {
    return error::InvalidArgument("Couldn't find [$0] in relation.",
                                  absl::StrJoin(missing_cols, "\n"));
  }
  return Status::OK();
}

StatusOr<types::DataType> IRRelationHandler::EvaluateColExpr(
    ColumnIR* expr, const table_store::schema::Relation& parent_rel) {
  // Update the column properties from the parent_rel
  if (!parent_rel.HasColumn(expr->col_name())) {
    return error::InvalidArgument("Couldn't find column $0 in relation.", expr->col_name());
  }
  types::DataType data_type = parent_rel.GetColumnType(expr->col_name());
  int64_t col_idx = parent_rel.GetColumnIndex(expr->col_name());
  expr->SetColumnIdx(col_idx);
  expr->SetColumnType(data_type);
  return data_type;
}

StatusOr<types::DataType> IRRelationHandler::EvaluateFuncExpr(
    FuncIR* expr, const table_store::schema::Relation& parent_rel, bool is_map) {
  // Evaluate the args
  std::vector<types::DataType> args_types;
  for (auto& arg : expr->args()) {
    PL_ASSIGN_OR_RETURN(auto arg_type, EvaluateExpression(arg, parent_rel, is_map));
    args_types.push_back(arg_type);
  }
  types::DataType data_type;
  // set the type of the function in the UDF.
  if (is_map) {
    PL_ASSIGN_OR_RETURN(data_type,
                        compiler_state_->registry_info()->GetUDF(expr->func_name(), args_types));
    expr->set_func_id(compiler_state_->GetUDFID(RegistryKey(expr->func_name(), args_types)));
  } else {
    // Check in UDA instead.
    PL_ASSIGN_OR_RETURN(data_type,
                        compiler_state_->registry_info()->GetUDA(expr->func_name(), args_types));
    expr->set_func_id(compiler_state_->GetUDAID(RegistryKey(expr->func_name(), args_types)));
  }

  return data_type;
}

StatusOr<types::DataType> IRRelationHandler::EvaluateExpression(
    IRNode* expr, const table_store::schema::Relation& parent_rel, bool is_map) {
  types::DataType data_type;
  switch (expr->type()) {
    case IRNodeType::ColumnType: {
      // Update the column properties from the parent_rel
      auto* col_expr = static_cast<ColumnIR*>(expr);
      PL_ASSIGN_OR_RETURN(data_type, EvaluateColExpr(col_expr, parent_rel));
      break;
    }
    case IRNodeType::FuncType: {
      auto* func_expr = static_cast<FuncIR*>(expr);
      PL_ASSIGN_OR_RETURN(data_type, EvaluateFuncExpr(func_expr, parent_rel, is_map));
      break;
    }
    case IRNodeType::IntType: {
      data_type = types::DataType::INT64;
      break;
    }
    case IRNodeType::StringType: {
      data_type = types::DataType::STRING;
      break;
    }
    case IRNodeType::FloatType: {
      data_type = types::DataType::FLOAT64;
      break;
    }
    case IRNodeType::BoolType: {
      data_type = types::DataType::BOOLEAN;
      break;
    }
    case IRNodeType::TimeType: {
      data_type = types::DataType::TIME64NS;
      break;
    }
    default: {
      return error::InvalidArgument("Didn't expect node of type $0 in expression evaluator.",
                                    expr->type_string());
    }
  }
  return data_type;
}
// Get the types of the children
// Check the registry for function names
StatusOr<table_store::schema::Relation> IRRelationHandler::BlockingAggHandler(
    OperatorIR* node, table_store::schema::Relation parent_rel) {
  DCHECK_EQ(node->type(), IRNodeType::BlockingAggType);
  auto agg_node = static_cast<BlockingAggIR*>(node);

  DCHECK_EQ(agg_node->agg_func()->type(), IRNodeType::LambdaType);
  auto* agg_func = static_cast<LambdaIR*>(agg_node->agg_func());

  // Make sure that the expected columns exist in the parent_relation.
  auto agg_expected = agg_func->expected_column_names();
  PL_RETURN_IF_ERROR(HasExpectedColumns(agg_expected, parent_rel));

  table_store::schema::Relation agg_rel;
  // TODO(philkuz) move this handler to a helper to simplify code here.
  if (agg_node->by_func() != nullptr) {
    LambdaIR* by_func = static_cast<LambdaIR*>(agg_node->by_func());
    auto by_expected = by_func->expected_column_names();
    PL_RETURN_IF_ERROR(HasExpectedColumns(by_expected, parent_rel));
    // Get the column to group by.
    PL_ASSIGN_OR_RETURN(IRNode * expr, by_func->GetDefaultExpr());
    if (expr->type() == IRNodeType::ColumnType) {
      auto* col_expr = static_cast<ColumnIR*>(expr);
      // Make sure that the column is setup.
      PL_RETURN_IF_ERROR(EvaluateColExpr(col_expr, parent_rel));
      agg_node->SetGroups({col_expr});
      agg_rel.AddColumn(col_expr->type(), col_expr->col_name());
    } else if (expr->type() == IRNodeType::ListType) {
      auto* list_expr = static_cast<ListIR*>(expr);
      std::vector<ColumnIR*> columns;
      for (auto ch : list_expr->children()) {
        auto* col_expr = static_cast<ColumnIR*>(ch);
        PL_RETURN_IF_ERROR(EvaluateColExpr(col_expr, parent_rel));
        columns.push_back(col_expr);
      }
      agg_node->SetGroups(columns);
      for (auto c : columns) {
        agg_rel.AddColumn(c->type(), c->col_name());
      }
    } else {
      return IRUtils::CreateIRNodeError(
          absl::StrFormat("Expected a 'Column' or 'List' for the by function body, got '%s",
                          expr->type_string()),
          *node);
    }
  } else {
    // if by_func not set, group by all.
    agg_node->SetGroups({});
  }

  // Make a new relation with each of the expression key, type pairs.
  ColExpressionVector col_exprs = agg_func->col_exprs();
  for (auto& entry : col_exprs) {
    std::string col_name = entry.name;
    PL_ASSIGN_OR_RETURN(types::DataType col_type,
                        EvaluateExpression(entry.node, parent_rel, false));
    agg_rel.AddColumn(col_type, col_name);
  }
  agg_node->SetAggValMap(col_exprs);

  PL_RETURN_IF_ERROR(agg_node->SetRelation(agg_rel));
  return agg_rel;
}

StatusOr<table_store::schema::Relation> IRRelationHandler::MapHandler(
    OperatorIR* node, table_store::schema::Relation parent_rel) {
  DCHECK_EQ(node->type(), IRNodeType::MapType);
  auto map_node = static_cast<MapIR*>(node);
  DCHECK_EQ(map_node->lambda_func()->type(), IRNodeType::LambdaType);
  auto* lambda_func = static_cast<LambdaIR*>(map_node->lambda_func());

  // Make sure that the expected columns exist in the parent_relation.
  auto lambda_expected = lambda_func->expected_column_names();
  PL_RETURN_IF_ERROR(HasExpectedColumns(lambda_expected, parent_rel));
  // Make a new relation with each of the expression key, type pairs.
  ColExpressionVector col_exprs = lambda_func->col_exprs();

  table_store::schema::Relation map_rel;
  for (auto& entry : col_exprs) {
    std::string col_name = entry.name;
    PL_ASSIGN_OR_RETURN(types::DataType col_type, EvaluateExpression(entry.node, parent_rel, true));
    map_rel.AddColumn(col_type, col_name);
  }
  map_node->SetColExprs(col_exprs);

  PL_RETURN_IF_ERROR(map_node->SetRelation(map_rel));
  return map_rel;
}

StatusOr<table_store::schema::Relation> IRRelationHandler::FilterHandler(
    OperatorIR* node, table_store::schema::Relation parent_rel) {
  DCHECK_EQ(node->type(), IRNodeType::FilterType);
  auto filter_node = static_cast<FilterIR*>(node);
  DCHECK_EQ(filter_node->filter_func()->type(), IRNodeType::LambdaType);
  auto* lambda_func = static_cast<LambdaIR*>(filter_node->filter_func());

  // Make sure that the expected columns exist in the parent_relation.
  auto lambda_expected = lambda_func->expected_column_names();
  PL_RETURN_IF_ERROR(HasExpectedColumns(lambda_expected, parent_rel));
  PL_ASSIGN_OR_RETURN(auto expr, lambda_func->GetDefaultExpr());
  PL_ASSIGN_OR_RETURN(types::DataType col_type, EvaluateExpression(expr, parent_rel, true));
  if (col_type != types::DataType::BOOLEAN) {
    return IRUtils::CreateIRNodeError(
        absl::StrFormat("Only expecting Boolean type for the filter expression, not %s",
                        types::DataType_Name(col_type)),
        *lambda_func);
  }

  PL_RETURN_IF_ERROR(filter_node->SetRelation(parent_rel));
  return parent_rel;
}

StatusOr<table_store::schema::Relation> IRRelationHandler::LimitHandler(
    OperatorIR* node, table_store::schema::Relation parent_rel) {
  DCHECK_EQ(node->type(), IRNodeType::LimitType);
  auto limit_node = static_cast<LimitIR*>(node);
  DCHECK_EQ(limit_node->limit_node()->type(), IRNodeType::IntType);
  auto limit_value_node = static_cast<IntIR*>(limit_node->limit_node());

  limit_node->SetLimitValue(limit_value_node->val());
  return parent_rel;
}

StatusOr<table_store::schema::Relation> IRRelationHandler::SinkHandler(
    OperatorIR*, table_store::schema::Relation parent_rel) {
  return parent_rel;
}

StatusOr<IntIR*> IRRelationHandler::EvaluateCompilerFunction(const std::string& name,
                                                             std::vector<IntIR*> evaled_args,
                                                             IRNode* parent_node) {
  if (evaled_args.size() != 2) {
    return IRUtils::CreateIRNodeError(
        absl::StrFormat("Expected 2 argument to %s call, got %d.", name, evaled_args.size()),
        *parent_node);
  }
  int64_t result = 0;
  // TODO(philkuz) Reviewer: any ideas how can we make this cleaner?
  if (name == "plc.multiply") {
    result = 1;
    for (auto a : evaled_args) {
      result *= a->val();
    }
  } else if (name == "plc.add") {
    for (auto a : evaled_args) {
      result += a->val();
    }
  } else if (name == "plc.subtract") {
    result = evaled_args[0]->val() - evaled_args[1]->val();
  } else {
    return IRUtils::CreateIRNodeError(
        absl::StrFormat("Only allowing [multiply, add, subtract], not %s", name), *parent_node);
  }
  PL_ASSIGN_OR_RETURN(IntIR * ir_result, parent_node->graph_ptr()->MakeNode<IntIR>());
  PL_RETURN_IF_ERROR(ir_result->Init(result, parent_node->ast_node()));
  return ir_result;
}

StatusOr<IntIR*> IRRelationHandler::EvaluateCompilerExpression(IRNode* node) {
  if (node->type() == IRNodeType::FuncType) {
    auto func_node = static_cast<FuncIR*>(node);
    std::vector<IntIR*> evaled_args;
    for (const auto ag : func_node->args()) {
      PL_ASSIGN_OR_RETURN(auto eval_result, EvaluateCompilerExpression(ag));
      evaled_args.push_back(eval_result);
    }
    PL_ASSIGN_OR_RETURN(auto node_result,
                        EvaluateCompilerFunction(func_node->func_name(), evaled_args, node));
    return node_result;

  } else if (node->type() == IRNodeType::IntType) {
    return static_cast<IntIR*>(node);
  } else if (node->type() == IRNodeType::StringType) {
    // Do the string processing
    auto str_node = static_cast<StringIR*>(node);
    PL_ASSIGN_OR_RETURN(int64_t int_val, StringToTimeInt(str_node->str()));
    int64_t time_repr = compiler_state_->time_now().val + int_val;
    PL_ASSIGN_OR_RETURN(auto out_node, node->graph_ptr()->MakeNode<IntIR>());
    PL_RETURN_IF_ERROR(out_node->Init(time_repr, node->ast_node()));
    return out_node;
  }
  return IRUtils::CreateIRNodeError(
      absl::Substitute("Expected time constant or expression, not $0", node->type_string()), *node);
}

StatusOr<table_store::schema::Relation> IRRelationHandler::RangeHandler(
    OperatorIR* node, table_store::schema::Relation parent_rel) {
  DCHECK_EQ(node->type(), IRNodeType::RangeType);
  auto range_ir = static_cast<RangeIR*>(node);
  PL_ASSIGN_OR_RETURN(IntIR * new_start_repr, EvaluateCompilerExpression(range_ir->start_repr()));
  PL_ASSIGN_OR_RETURN(IntIR * new_stop_repr, EvaluateCompilerExpression(range_ir->stop_repr()));
  PL_RETURN_IF_ERROR(range_ir->SetStartStop(new_start_repr, new_stop_repr));
  return parent_rel;
}

Status IRRelationHandler::RelationUpdate(OperatorIR* node) {
  if (!node->HasParent()) {
    return error::InvalidArgument(
        "The $0 node (id=$1) has no parent. This means that the relation was not initialized "
        "correctlly for $0",
        node->type_string(), node->id());
  }

  // Get the parents relation, or update it if necessary
  auto parent = node->parent();
  if (!parent->IsRelationInit()) {
    PL_RETURN_IF_ERROR(RelationUpdate(parent));
  }
  // with the relation, now do the appropriate thing for  it.
  table_store::schema::Relation parent_rel = parent->relation();
  table_store::schema::Relation rel;
  // TODO(philkuz) (PL-466) update the arguments to each handler to only take in the specific
  // type so you don't have to recast it.
  switch (node->type()) {
    case IRNodeType::MemorySinkType: {
      PL_ASSIGN_OR_RETURN(rel, SinkHandler(node, parent_rel));
      break;
    }
    case IRNodeType::BlockingAggType: {
      PL_ASSIGN_OR_RETURN(rel, BlockingAggHandler(node, parent_rel));
      break;
    }
    case IRNodeType::MapType: {
      PL_ASSIGN_OR_RETURN(rel, MapHandler(node, parent_rel));
      break;
    }
    case IRNodeType::RangeType: {
      PL_ASSIGN_OR_RETURN(rel, RangeHandler(node, parent_rel));
      break;
    }
    case IRNodeType::FilterType: {
      PL_ASSIGN_OR_RETURN(rel, FilterHandler(node, parent_rel));
      break;
    }
    case IRNodeType::LimitType: {
      PL_ASSIGN_OR_RETURN(rel, LimitHandler(node, parent_rel));
      break;
    }
    default: { return error::InvalidArgument("Couldn't find handler for $0", node->type_string()); }
  }
  return node->SetRelation(rel);
}

Status IRRelationHandler::SetSourceRelation(IRNode* node) {
  if (node->type() != MemorySourceType) {
    return error::InvalidArgument("Only implemented MemorySourceType, can't handle $0",
                                  node->type_string());
  }
  auto mem_node = static_cast<MemorySourceIR*>(node);
  auto table_node = mem_node->table_node();
  auto select = mem_node->select();
  if (table_node->type() != StringType) {
    return error::InvalidArgument("table argument only implemented for string type.");
  }
  if (select->type() != ListType) {
    return error::InvalidArgument("select argument only implemented for list type.");
  }
  auto table_str = static_cast<StringIR*>(table_node)->str();
  // get the table_str from the relation map
  auto relation_map_it = compiler_state_->relation_map()->find(table_str);
  if (relation_map_it == compiler_state_->relation_map()->end()) {
    return error::InvalidArgument("Table $0 not found in the relation map", table_str);
  }
  table_store::schema::Relation table_relation = relation_map_it->second;

  // get the children.
  auto select_children = static_cast<ListIR*>(select)->children();
  std::vector<std::string> columns;
  for (auto& col_string_node : select_children) {
    if (col_string_node->type() != StringType) {
      return error::InvalidArgument("select children should be strings.");
    }
    columns.push_back(static_cast<StringIR*>(col_string_node)->str());
  }
  PL_ASSIGN_OR_RETURN(auto select_relation, table_relation.MakeSubRelation(columns));

  PL_ASSIGN_OR_RETURN(auto cols, GetColumnsFromRelation(node, columns, table_relation));
  mem_node->SetColumns(cols);
  return mem_node->SetRelation(select_relation);
}

StatusOr<std::vector<ColumnIR*>> IRRelationHandler::GetColumnsFromRelation(
    IRNode* node, std::vector<std::string> col_names,
    const table_store::schema::Relation& relation) {
  auto graph = node->graph_ptr();
  auto result = std::vector<ColumnIR*>();
  // iterates through the columns, finds their relation position,
  // then create columns with index and type.
  for (const auto& col_name : col_names) {
    int64_t i = relation.GetColumnIndex(col_name);
    PL_ASSIGN_OR_RETURN(auto col_node, graph->MakeNode<ColumnIR>());
    PL_RETURN_IF_ERROR(col_node->Init(col_name, node->ast_node()));
    col_node->SetColumnIdx(i);
    col_node->SetColumnType(relation.GetColumnType(i));
    result.push_back(col_node);
  }
  return result;
}

Status IRRelationHandler::SetAllSourceRelations(IR* ir_graph) {
  for (auto& i : ir_graph->dag().TopologicalSort()) {
    auto node = ir_graph->Get(i);
    if (node->is_source()) {
      PL_RETURN_IF_ERROR(SetSourceRelation(node));
    }
  }
  return Status::OK();
}

Status IRRelationHandler::UpdateRelationsAndCheckFunctions(IR* ir_graph) {
  // Get the source relations.
  PL_RETURN_IF_ERROR(SetAllSourceRelations(ir_graph));
  // Currently only
  PL_ASSIGN_OR_RETURN(auto node, ir_graph->GetSink());
  DCHECK(node->IsOp());
  // Get the sink node
  auto sink_node = static_cast<OperatorIR*>(node);
  // Start the relation update at the sink nodes.
  return RelationUpdate(sink_node);
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
