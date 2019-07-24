#include <memory>
#include <string>
#include <vector>

#include "src/carnot/compiler/rules.h"
namespace pl {
namespace carnot {
namespace compiler {

StatusOr<bool> Rule::Execute(IR* ir_graph) const {
  std::vector<int64_t> topo_graph = ir_graph->dag().TopologicalSort();
  bool any_changed = false;
  for (int64_t node_i : topo_graph) {
    PL_ASSIGN_OR_RETURN(bool node_is_changed, Apply(ir_graph->Get(node_i)));
    any_changed = any_changed || node_is_changed;
  }
  return any_changed;
}

StatusOr<bool> DataTypeRule::Apply(IRNode* ir_node) const {
  if (match(ir_node, UnresolvedRTFuncMatchAllArgs(ResolvedExpression()))) {
    // Match any function that has all args resolved.
    return EvaluateFunc(static_cast<FuncIR*>(ir_node));
  } else if (match(ir_node, UnresolvedFuncType())) {
    // Matches any function that has some unresolved args.
    VLOG(1) << absl::Substitute("$1(id=$0) has unresolved args.", ir_node->id(),
                                ir_node->type_string());
  } else if (match(ir_node, UnresolvedColumnType())) {
    // Evaluate any unresolved columns.
    return EvaluateColumn(static_cast<ColumnIR*>(ir_node));
  } else if (match(ir_node, UnresolvedMetadataType())) {
    // Evaluate any unresolved columns.
    return EvaluateColumn(static_cast<ColumnIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> DataTypeRule::EvaluateFunc(FuncIR* func) const {
  // Get the types of the children of this function.
  std::vector<types::DataType> children_data_types;
  for (const auto& arg : func->args()) {
    types::DataType t = arg->EvaluatedDataType();
    DCHECK(t != types::DataType::DATA_TYPE_UNKNOWN);
    children_data_types.push_back(t);
  }
  PL_ASSIGN_OR_RETURN(IRNode * containing_op, func->ContainingOp());
  IRNodeType containing_op_type = containing_op->type();
  if (containing_op_type != IRNodeType::kBlockingAgg) {
    // Attempt to resolve UDF function for non-Aggregate nodes.
    PL_ASSIGN_OR_RETURN(types::DataType data_type, compiler_state_->registry_info()->GetUDF(
                                                       func->func_name(), children_data_types));
    func->set_func_id(
        compiler_state_->GetUDFID(RegistryKey(func->func_name(), children_data_types)));
    func->SetOutputDataType(data_type);
  } else {
    // Attempt to resolve UDA function for Aggregate nodes.
    PL_ASSIGN_OR_RETURN(types::DataType data_type, compiler_state_->registry_info()->GetUDA(
                                                       func->func_name(), children_data_types));
    func->set_func_id(
        compiler_state_->GetUDAID(RegistryKey(func->func_name(), children_data_types)));
    func->SetOutputDataType(data_type);
  }
  func->SetArgsTypes(children_data_types);
  return true;
}

StatusOr<bool> DataTypeRule::EvaluateColumn(ColumnIR* column) const {
  PL_ASSIGN_OR_RETURN(OperatorIR * container_op, column->ContainingOp());
  if (!container_op->HasParent()) {
    return column->CreateIRNodeError("No parent for operator $1(id=$2). Can't resolve column '$0'.",
                                     column->col_name(), container_op->type_string(),
                                     container_op->id());
  }

  OperatorIR* parent_op = container_op->parent();
  if (!parent_op->IsRelationInit()) {
    // Missing a relation in parent op is not a failure, it means the parent op still has to
    // propogate results.
    VLOG(1) << absl::Substitute("Have yet to evaluate relation for operator $1(id=$0)",
                                parent_op->id(), parent_op->type_string());
    return false;
  }

  // Get the parent relation and find the column in it.
  table_store::schema::Relation relation = parent_op->relation();
  if (!relation.HasColumn(column->col_name())) {
    return column->CreateIRNodeError("Column '$0' not found in relation of $1(id=$2)",
                                     column->col_name(), parent_op->type_string(), parent_op->id());
  }
  types::DataType col_type = relation.GetColumnType(column->col_name());
  int64_t col_idx = relation.GetColumnIndex(column->col_name());
  column->ResolveColumn(col_idx, col_type);

  return true;
}

StatusOr<bool> SourceRelationRule::Apply(IRNode* ir_node) const {
  if (match(ir_node, UnresolvedSource())) {
    return GetSourceRelation(static_cast<OperatorIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> SourceRelationRule::GetSourceRelation(OperatorIR* source_op) const {
  if (source_op->type() != IRNodeType::kMemorySource) {
    return source_op->CreateIRNodeError(
        "Object $0(id=$1) not treated as a Source Op. No relation could be mapped.",
        source_op->type_string(), source_op->id());
  }
  MemorySourceIR* mem_node = static_cast<MemorySourceIR*>(source_op);
  ListIR* select = mem_node->select();
  std::string table_str = mem_node->table_name();
  // get the table_str from the relation map
  auto relation_map_it = compiler_state_->relation_map()->find(table_str);
  if (relation_map_it == compiler_state_->relation_map()->end()) {
    return mem_node->CreateIRNodeError("Table '$0' not found.", table_str);
  }
  table_store::schema::Relation table_relation = relation_map_it->second;
  // get the children.
  std::vector<std::string> columns;
  table_store::schema::Relation select_relation;
  if (!mem_node->select_all()) {
    PL_ASSIGN_OR_RETURN(columns, GetColumnNames(select->children()));
    PL_ASSIGN_OR_RETURN(select_relation, GetSelectRelation(select, table_relation, columns));
  } else {
    columns = table_relation.col_names();
    select_relation = table_relation;
  }
  PL_ASSIGN_OR_RETURN(std::vector<ColumnIR*> cols,
                      GetColumnsFromRelation(mem_node, columns, table_relation));
  mem_node->SetColumns(cols);
  PL_RETURN_IF_ERROR(mem_node->SetRelation(select_relation));
  return true;
}

StatusOr<table_store::schema::Relation> SourceRelationRule::GetSelectRelation(
    IRNode* node, const table_store::schema::Relation& relation,
    const std::vector<std::string>& columns) const {
  table_store::schema::Relation new_relation;
  std::vector<std::string> missing_columns;
  for (auto& c : columns) {
    if (!relation.HasColumn(c)) {
      missing_columns.push_back(c);
      continue;
    }
    auto col_type = relation.GetColumnType(c);
    new_relation.AddColumn(col_type, c);
  }
  if (missing_columns.size() > 0) {
    return node->CreateIRNodeError("Columns {$0} are missing in table.",
                                   absl::StrJoin(missing_columns, ","));
  }
  return new_relation;
}

StatusOr<std::vector<ColumnIR*>> SourceRelationRule::GetColumnsFromRelation(
    IRNode* node, std::vector<std::string> col_names,
    const table_store::schema::Relation& relation) const {
  auto graph = node->graph_ptr();
  auto result = std::vector<ColumnIR*>();
  // iterates through the columns, finds their relation position,
  // then create columns with index and type.
  for (const auto& col_name : col_names) {
    int64_t i = relation.GetColumnIndex(col_name);
    PL_ASSIGN_OR_RETURN(auto col_node, graph->MakeNode<ColumnIR>());
    PL_RETURN_IF_ERROR(col_node->Init(col_name, node->ast_node()));
    col_node->ResolveColumn(i, relation.GetColumnType(i));
    result.push_back(col_node);
  }
  return result;
}

StatusOr<std::vector<std::string>> SourceRelationRule::GetColumnNames(
    std::vector<ExpressionIR*> select_children) const {
  std::vector<std::string> columns;
  for (size_t idx = 0; idx < select_children.size(); idx++) {
    IRNode* col_string_node = select_children[idx];
    if (col_string_node->type() != IRNodeType::kString) {
      return col_string_node->CreateIRNodeError(
          "The elements of the select list must be of type `str`. Found a '$0' for idx $1.",
          col_string_node->type_string(), idx);
    }
    columns.push_back(static_cast<StringIR*>(col_string_node)->str());
  }
  return columns;
}

StatusOr<bool> OperatorRelationRule::Apply(IRNode* ir_node) const {
  if (match(ir_node, UnresolvedReadyBlockingAgg())) {
    return SetBlockingAgg(static_cast<BlockingAggIR*>(ir_node));
  } else if (match(ir_node, UnresolvedReadyMap())) {
    return SetMap(static_cast<MapIR*>(ir_node));
  } else if (match(ir_node, UnresolvedReadyMetadataResolver())) {
    return SetMetadataResolver(static_cast<MetadataResolverIR*>(ir_node));
  } else if (match(ir_node, UnresolvedReadyOp())) {
    return SetOther(static_cast<OperatorIR*>(ir_node));
  }
  return false;
}
bool UpdateColumn(ColumnIR* col_expr, std::vector<ColumnIR*>* columns,
                  table_store::schema::Relation* relation_ptr) {
  if (!col_expr->IsDataTypeEvaluated()) {
    return false;
  }
  relation_ptr->AddColumn(col_expr->EvaluatedDataType(), col_expr->col_name());
  columns->push_back(col_expr);
  return true;
}

StatusOr<bool> OperatorRelationRule::SetBlockingAgg(BlockingAggIR* agg_ir) const {
  table_store::schema::Relation agg_rel;
  std::vector<ColumnIR*> groups;
  if (!agg_ir->group_by_all()) {
    PL_ASSIGN_OR_RETURN(IRNode * default_expr, agg_ir->by_func()->GetDefaultExpr());
    if (!default_expr->IsExpression()) {
      return agg_ir->CreateIRNodeError("Expected an expression, not a '$0'.",
                                       default_expr->type_string());
    }
    ExpressionIR* expr = static_cast<ExpressionIR*>(default_expr);
    if (expr->IsColumn()) {
      if (!UpdateColumn(static_cast<ColumnIR*>(expr), &groups, &agg_rel)) {
        return false;
      }
    } else if (expr->type() == IRNodeType::kList) {
      for (auto ch : static_cast<ListIR*>(expr)->children()) {
        DCHECK(ch->IsColumn()) << "Expect group by to be column.";
        if (!UpdateColumn(static_cast<ColumnIR*>(ch), &groups, &agg_rel)) {
          return false;
        }
      }
    } else {
      return agg_ir->CreateIRNodeError(
          "Expected a 'Column' or 'List' for the by function body, got '$0", expr->type_string());
    }
  }

  // Make a new relation with each of the expression key, type pairs.
  ColExpressionVector col_exprs = agg_ir->agg_func()->col_exprs();
  for (auto& entry : col_exprs) {
    std::string col_name = entry.name;
    if (!entry.node->IsDataTypeEvaluated()) {
      return false;
    }
    agg_rel.AddColumn(entry.node->EvaluatedDataType(), col_name);
  }

  PL_RETURN_IF_ERROR(agg_ir->SetRelation(agg_rel));
  agg_ir->SetGroups(groups);
  agg_ir->SetAggValMap(col_exprs);
  return true;
}
StatusOr<bool> OperatorRelationRule::SetMap(MapIR* map_ir) const {
  table_store::schema::Relation map_rel;
  // Make a new relation with each of the expression key, type pairs.
  ColExpressionVector col_exprs = map_ir->lambda_func()->col_exprs();
  for (auto& entry : col_exprs) {
    std::string col_name = entry.name;
    if (!entry.node->IsDataTypeEvaluated()) {
      return false;
    }
    map_rel.AddColumn(entry.node->EvaluatedDataType(), col_name);
  }
  map_ir->SetColExprs(col_exprs);
  PL_RETURN_IF_ERROR(map_ir->SetRelation(map_rel));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetMetadataResolver(MetadataResolverIR* md_ir) const {
  table_store::schema::Relation md_rel = md_ir->parent()->relation();
  // Iterate through the columns and add them in.
  for (const auto& col_entry : md_ir->metadata_columns()) {
    md_rel.AddColumn(col_entry.second->column_type(), col_entry.second->column_name_repr());
  }
  PL_RETURN_IF_ERROR(md_ir->SetRelation(md_rel));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetOther(OperatorIR* operator_ir) const {
  PL_RETURN_IF_ERROR(operator_ir->SetRelation(operator_ir->parent()->relation()));
  return true;
}

StatusOr<bool> RangeArgExpressionRule::Apply(IRNode* ir_node) const {
  PL_UNUSED(ir_node);
  if (match(ir_node, Range(Int(), Int()))) {
    return false;
  } else if (match(ir_node, Range())) {
    RangeIR* range = static_cast<RangeIR*>(ir_node);
    IRNode* start = range->start_repr();
    IRNode* stop = range->stop_repr();
    PL_ASSIGN_OR_RETURN(start, EvalExpression(start));
    PL_ASSIGN_OR_RETURN(stop, EvalExpression(stop));
    PL_RETURN_IF_ERROR(range->SetStartStop(start, stop));
    return true;
  }
  return false;
}

StatusOr<IntIR*> RangeArgExpressionRule::EvalExpression(IRNode* node) const {
  if (match(node, Int())) {
    return static_cast<IntIR*>(node);
  } else if (match(node, CompileTimeFunc())) {
    auto func_node = static_cast<FuncIR*>(node);
    std::vector<IntIR*> evaled_args;
    for (const auto ag : func_node->args()) {
      PL_ASSIGN_OR_RETURN(auto eval_result, EvalExpression(ag));
      evaled_args.push_back(eval_result);
    }
    PL_ASSIGN_OR_RETURN(auto node_result, EvalFunc(func_node->func_name(), evaled_args, func_node));
    return node_result;
  } else if (match(node, String())) {
    // Do the string processing
    auto str_node = static_cast<StringIR*>(node);
    // TODO(philkuz) (PL-708) make StringToTimeInt also take time_now as an argument.
    PL_ASSIGN_OR_RETURN(int64_t int_val, StringToTimeInt(str_node->str()));
    int64_t time_repr = compiler_state_->time_now().val + int_val;
    PL_ASSIGN_OR_RETURN(auto out_node, node->graph_ptr()->MakeNode<IntIR>());
    PL_RETURN_IF_ERROR(out_node->Init(time_repr, node->ast_node()));
    return out_node;
  }
  return node->CreateIRNodeError(
      "Expected integer, time expression, or a string representation of time, not $0",
      node->type_string());
}
StatusOr<IntIR*> RangeArgExpressionRule::EvalFunc(std::string name, std::vector<IntIR*> evaled_args,
                                                  FuncIR* func) const {
  if (evaled_args.size() != 2) {
    return func->CreateIRNodeError("Expected 2 argument to $0 call, got $1.", name,
                                   evaled_args.size());
  }
  int64_t result = 0;
  // TODO(philkuz) (PL-709) Make a UDCF (C := CompileTime) to combine these together.
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
    return func->CreateIRNodeError("Only allowing [multiply, add, subtract], not $0", name);
  }
  PL_ASSIGN_OR_RETURN(IntIR * ir_result, func->graph_ptr()->MakeNode<IntIR>());
  PL_RETURN_IF_ERROR(ir_result->Init(result, func->ast_node()));
  return ir_result;
}

StatusOr<bool> VerifyFilterExpressionRule::Apply(IRNode* ir_node) const {
  if (match(ir_node, Filter())) {
    // Match any function that has all args resolved.
    FilterIR* filter = static_cast<FilterIR*>(ir_node);
    LambdaIR* lambda = filter->filter_func();
    PL_ASSIGN_OR_RETURN(IRNode * f, lambda->GetDefaultExpr());
    DCHECK(f->IsExpression()) << "Expected expression, but didn't find it.";
    types::DataType expr_type = static_cast<ExpressionIR*>(f)->EvaluatedDataType();
    if (expr_type != types::DataType::BOOLEAN) {
      return ir_node->CreateIRNodeError("Expected Boolean for Filter expression, got $0",
                                        types::DataType_Name(expr_type));
    }
  }
  return false;
}

StatusOr<bool> ResolveMetadataRule::Apply(IRNode* ir_node) const {
  if (match(ir_node, UnresolvedMetadataIR())) {
    // Match any function that has all args resolved.
    return HandleMetadata(static_cast<MetadataIR*>(ir_node));
  }
  return false;
}

StatusOr<MetadataResolverIR*> ResolveMetadataRule::InsertMetadataResolver(
    OperatorIR* container_op, OperatorIR* parent_op) const {
  DCHECK_EQ(container_op->parent()->id(), parent_op->id())
      << "Parent arg should be the actual parent of the container_op.";
  IR* graph = container_op->graph_ptr();
  PL_ASSIGN_OR_RETURN(auto md_resolver, graph->MakeNode<MetadataResolverIR>());
  PL_RETURN_IF_ERROR(md_resolver->Init(parent_op, {{}}, container_op->ast_node()));
  PL_RETURN_IF_ERROR(container_op->RemoveParent(parent_op));
  PL_RETURN_IF_ERROR(container_op->SetParent(md_resolver));
  return md_resolver;
}

StatusOr<bool> ResolveMetadataRule::HandleMetadata(MetadataIR* metadata) const {
  // Get containing operator.
  PL_ASSIGN_OR_RETURN(OperatorIR * container_op, metadata->ContainingOp());
  if (!container_op->HasParent()) {
    return metadata->CreateIRNodeError(
        "No parent for operator $1(id=$2). Can't resolve column '$0'.", metadata->col_name(),
        container_op->type_string(), container_op->id());
  }

  OperatorIR* parent_op = container_op->parent();
  if (parent_op->type() != IRNodeType::kMetadataResolver) {
    // If the parent is not a metadata resolver, add a parent metadata resolver node.
    PL_ASSIGN_OR_RETURN(parent_op, InsertMetadataResolver(container_op, parent_op));
  }
  auto md_resolver_op = static_cast<MetadataResolverIR*>(parent_op);

  // Check to see whether metadata is valid.
  if (!md_handler_->HasProperty(metadata->name())) {
    return metadata->CreateIRNodeError("Specified metadata value '$0' is not properly handled.",
                                       metadata->name());
  }
  PL_ASSIGN_OR_RETURN(MetadataProperty * md_property, md_handler_->GetProperty(metadata->name()));
  PL_RETURN_IF_ERROR(metadata->ResolveMetadataColumn(md_resolver_op, md_property));
  PL_RETURN_IF_ERROR(md_resolver_op->AddMetadata(md_property));

  return true;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
