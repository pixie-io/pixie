#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/rules.h"
namespace pl {
namespace carnot {
namespace compiler {

StatusOr<bool> Rule::Execute(IR* ir_graph) {
  std::vector<int64_t> topo_graph = ir_graph->dag().TopologicalSort();
  bool any_changed = false;
  for (int64_t node_i : topo_graph) {
    PL_ASSIGN_OR_RETURN(bool node_is_changed, Apply(ir_graph->Get(node_i)));
    any_changed = any_changed || node_is_changed;
  }
  while (!node_delete_q.empty()) {
    PL_RETURN_IF_ERROR(ir_graph->DeleteNode(node_delete_q.front()));
    node_delete_q.pop();
  }
  return any_changed;
}

void Rule::DeferNodeDeletion(int64_t node) { node_delete_q.push(node); }

StatusOr<bool> DataTypeRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedRTFuncMatchAllArgs(ResolvedExpression()))) {
    // Match any function that has all args resolved.
    return EvaluateFunc(static_cast<FuncIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedColumnType())) {
    // Evaluate any unresolved columns.
    return EvaluateColumn(static_cast<ColumnIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedMetadataType())) {
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
  PL_ASSIGN_OR_RETURN(IRNode * containing_op, func->ContainingOperator());
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
  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op, column->ReferencedOperator());
  if (!parent_op->IsRelationInit()) {
    // Missing a relation in parent op is not a failure, it means the parent op still has to
    // propogate results.
    return false;
  }

  // Get the parent relation and find the column in it.
  Relation relation = parent_op->relation();
  if (!relation.HasColumn(column->col_name())) {
    return column->CreateIRNodeError("Column '$0' not found in relation of $1(id=$2)",
                                     column->col_name(), parent_op->type_string(), parent_op->id());
  }
  types::DataType col_type = relation.GetColumnType(column->col_name());
  int64_t col_idx = relation.GetColumnIndex(column->col_name());
  column->ResolveColumn(col_idx, col_type);
  return true;
}

StatusOr<bool> SourceRelationRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedSource())) {
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
  std::string table_str = mem_node->table_name();
  // get the table_str from the relation map
  auto relation_map_it = compiler_state_->relation_map()->find(table_str);
  if (relation_map_it == compiler_state_->relation_map()->end()) {
    return mem_node->CreateIRNodeError("Table '$0' not found.", table_str);
  }
  Relation table_relation = relation_map_it->second;
  // get the children.
  std::vector<std::string> columns;
  Relation select_relation;
  if (!mem_node->select_all()) {
    columns = mem_node->column_names();
    PL_ASSIGN_OR_RETURN(select_relation, GetSelectRelation(mem_node, table_relation, columns));
  } else {
    columns = table_relation.col_names();
    select_relation = table_relation;
  }
  PL_ASSIGN_OR_RETURN(std::vector<ColumnIR*> cols,
                      GetColumnsFromRelation(mem_node, columns, table_relation));
  PL_RETURN_IF_ERROR(mem_node->SetColumns(cols));
  PL_RETURN_IF_ERROR(mem_node->SetRelation(select_relation));
  return true;
}

StatusOr<Relation> SourceRelationRule::GetSelectRelation(
    IRNode* node, const Relation& relation, const std::vector<std::string>& columns) const {
  Relation new_relation;
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
    OperatorIR* node, std::vector<std::string> col_names, const Relation& relation) const {
  auto graph = node->graph_ptr();
  auto result = std::vector<ColumnIR*>();
  // iterates through the columns, finds their relation position,
  // then create columns with index and type.
  for (const auto& col_name : col_names) {
    int64_t i = relation.GetColumnIndex(col_name);
    PL_ASSIGN_OR_RETURN(auto col_node, graph->MakeNode<ColumnIR>());
    PL_RETURN_IF_ERROR(col_node->Init(col_name, /*parent_op_idx*/ 0, node->ast_node()));
    col_node->ResolveColumn(i, relation.GetColumnType(i));
    result.push_back(col_node);
  }
  return result;
}

StatusOr<bool> OperatorRelationRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedReadyBlockingAgg())) {
    return SetBlockingAgg(static_cast<BlockingAggIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyMap())) {
    return SetMap(static_cast<MapIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyMetadataResolver())) {
    return SetMetadataResolver(static_cast<MetadataResolverIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyUnion())) {
    return SetUnion(static_cast<UnionIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyJoin())) {
    return SetJoin(static_cast<JoinIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyOp())) {
    return SetOther(static_cast<OperatorIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> OperatorRelationRule::SetJoin(JoinIR* join_node) const {
  DCHECK_EQ(join_node->parents().size(), 2UL);
  OperatorIR* left = join_node->parents()[0];
  OperatorIR* right = join_node->parents()[1];

  Relation left_relation = left->relation();
  Relation right_relation = right->relation();

  Relation out_relation;

  for (size_t col_idx = 0; col_idx < join_node->output_columns().size(); ++col_idx) {
    const ColumnIR* col = join_node->output_columns()[col_idx];
    const std::string& new_col_name = join_node->column_names()[col_idx];
    Relation* col_relation;
    if (col->container_op_parent_idx() == 0) {
      col_relation = &left_relation;
    } else {
      col_relation = &right_relation;
    }

    out_relation.AddColumn(col_relation->GetColumnType(col->col_name()), new_col_name);
  }

  PL_RETURN_IF_ERROR(join_node->SetRelation(out_relation));
  return true;
}

bool UpdateColumn(ColumnIR* col_expr, Relation* relation_ptr) {
  if (!col_expr->IsDataTypeEvaluated()) {
    return false;
  }
  relation_ptr->AddColumn(col_expr->EvaluatedDataType(), col_expr->col_name());
  return true;
}

StatusOr<bool> OperatorRelationRule::SetBlockingAgg(BlockingAggIR* agg_ir) const {
  Relation agg_rel;
  for (ColumnIR* group : agg_ir->groups()) {
    if (!UpdateColumn(group, &agg_rel)) {
      return false;
    }
  }
  ColExpressionVector col_exprs = agg_ir->aggregate_expressions();
  for (auto& entry : col_exprs) {
    std::string col_name = entry.name;
    if (!entry.node->IsDataTypeEvaluated()) {
      return false;
    }
    agg_rel.AddColumn(entry.node->EvaluatedDataType(), col_name);
  }

  PL_RETURN_IF_ERROR(agg_ir->SetRelation(agg_rel));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetMap(MapIR* map_ir) const {
  Relation map_rel;
  // Make a new relation with each of the expression key, type pairs.
  for (auto& entry : map_ir->col_exprs()) {
    std::string col_name = entry.name;
    if (!entry.node->IsDataTypeEvaluated()) {
      return false;
    }
    map_rel.AddColumn(entry.node->EvaluatedDataType(), col_name);
  }
  PL_RETURN_IF_ERROR(map_ir->SetRelation(map_rel));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetMetadataResolver(MetadataResolverIR* md_ir) const {
  DCHECK_EQ(md_ir->parents().size(), 1UL);
  Relation md_rel = md_ir->parents()[0]->relation();
  // Iterate through the columns and add them in.
  for (const auto& col_entry : md_ir->metadata_columns()) {
    std::string column_name = col_entry.second->GetColumnRepr();
    if (md_rel.HasColumn(column_name)) {
      VLOG(2) << absl::Substitute(
          "Skipping '$0' in setting metadata resolver relation, already in relation", column_name);
      continue;
    }
    md_rel.AddColumn(col_entry.second->column_type(), column_name);
  }
  PL_RETURN_IF_ERROR(md_ir->SetRelation(md_rel));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetUnion(UnionIR* union_ir) const {
  PL_RETURN_IF_ERROR(union_ir->SetRelationFromParents());
  return true;
}

StatusOr<bool> OperatorRelationRule::SetOther(OperatorIR* operator_ir) const {
  CHECK_EQ(operator_ir->parents().size(), 1UL);
  PL_RETURN_IF_ERROR(operator_ir->SetRelation(operator_ir->parents()[0]->relation()));
  return true;
}

StatusOr<bool> RangeArgExpressionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, Range(Int(), Int()))) {
    // If Range matches this format, don't do any work.
    return false;
  } else if (Match(ir_node, Range())) {
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
  if (Match(node, Int())) {
    return static_cast<IntIR*>(node);
  } else if (Match(node, CompileTimeFunc())) {
    auto func_node = static_cast<FuncIR*>(node);
    std::vector<IntIR*> evaled_args;
    for (const auto ag : func_node->args()) {
      PL_ASSIGN_OR_RETURN(auto eval_result, EvalExpression(ag));
      evaled_args.push_back(eval_result);
    }
    PL_ASSIGN_OR_RETURN(auto node_result, EvalFunc(func_node->func_name(), evaled_args, func_node));
    return node_result;
  } else if (Match(node, String())) {
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

StatusOr<bool> VerifyFilterExpressionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, Filter())) {
    // Match any function that has all args resolved.
    FilterIR* filter = static_cast<FilterIR*>(ir_node);
    ExpressionIR* expr = filter->filter_expr();
    types::DataType expr_type = expr->EvaluatedDataType();
    if (expr_type != types::DataType::BOOLEAN) {
      return ir_node->CreateIRNodeError("Expected Boolean for Filter expression, got $0",
                                        types::DataType_Name(expr_type));
    }
  }
  return false;
}

StatusOr<bool> ResolveMetadataRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedMetadataIR())) {
    // Match any function that has all args resolved.
    return HandleMetadata(static_cast<MetadataIR*>(ir_node));
  }
  return false;
}

// Inserts the MetadataResolver before the container_op
StatusOr<MetadataResolverIR*> ResolveMetadataRule::InsertMetadataResolver(
    OperatorIR* container_op, OperatorIR* parent_op) const {
  DCHECK(container_op->IsChildOf(parent_op))
      << "Parent arg should be the actual parent of the container_op.";
  IR* graph = container_op->graph_ptr();
  PL_ASSIGN_OR_RETURN(auto md_resolver, graph->MakeNode<MetadataResolverIR>());
  // Metadata Resolver is now child of Parent.
  PL_RETURN_IF_ERROR(md_resolver->Init(parent_op, {{}}, container_op->ast_node()));
  // Previous operator is now child of Metadata Resolver.
  PL_RETURN_IF_ERROR(container_op->ReplaceParent(parent_op, md_resolver));

  return md_resolver;
}

StatusOr<bool> ResolveMetadataRule::HandleMetadata(MetadataIR* metadata) const {
  // Check to see whether metadata is valid.
  if (!md_handler_->HasProperty(metadata->name())) {
    return metadata->CreateIRNodeError("Specified metadata value '$0' is not properly handled.",
                                       metadata->name());
  }

  // Get containing operator.
  PL_ASSIGN_OR_RETURN(OperatorIR * container_op, metadata->ContainingOperator());
  if (!container_op->HasParents()) {
    return metadata->CreateIRNodeError(
        "No parent for operator $1(id=$2). Can't resolve column '$0'.", metadata->col_name(),
        container_op->type_string(), container_op->id());
  }

  MetadataResolverIR* md_resolver_op;
  PL_ASSIGN_OR_RETURN(OperatorIR * referenced_op, metadata->ReferencedOperator());
  if (referenced_op->type() == IRNodeType::kMetadataResolver) {
    md_resolver_op = static_cast<MetadataResolverIR*>(referenced_op);
  } else {
    // If the parent is not a metadata resolver, add a parent metadata resolver node.
    PL_ASSIGN_OR_RETURN(md_resolver_op, InsertMetadataResolver(container_op, referenced_op));
  }

  PL_ASSIGN_OR_RETURN(MetadataProperty * md_property, md_handler_->GetProperty(metadata->name()));
  PL_RETURN_IF_ERROR(metadata->ResolveMetadataColumn(md_resolver_op, md_property));
  PL_RETURN_IF_ERROR(md_resolver_op->AddMetadata(md_property));
  return true;
}

StatusOr<bool> MetadataFunctionFormatRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, Equals(Metadata(), MetadataLiteral()))) {
    // If the literal already matches, then no need to do any work.
    return false;
  } else if (Match(ir_node, Equals(Metadata(), Metadata()))) {
    return false;
  } else if (Match(ir_node, Equals(Metadata(), String()))) {
    FuncIR* func = static_cast<FuncIR*>(ir_node);
    StringIR* out_expr;
    MetadataIR* md_expr;
    int64_t update_idx;
    DCHECK_EQ(func->args().size(), 2UL);
    if (Match(func->args()[1], Metadata())) {
      update_idx = 0;
      out_expr = static_cast<StringIR*>(func->args()[0]);
      md_expr = static_cast<MetadataIR*>(func->args()[1]);
    } else {
      update_idx = 1;
      out_expr = static_cast<StringIR*>(func->args()[1]);
      md_expr = static_cast<MetadataIR*>(func->args()[0]);
    }

    DCHECK(md_expr->type() == IRNodeType::kMetadata) << absl::Substitute(
        "Expected Metadata at idx $1, found '$0.'", md_expr->type_string(), update_idx);

    PL_ASSIGN_OR_RETURN(MetadataLiteralIR * metadata_literal,
                        WrapLiteral(out_expr, md_expr->property()));
    PL_RETURN_IF_ERROR(func->UpdateArg(update_idx, metadata_literal));
    return true;
  } else if (Match(ir_node, FuncAnyArg(Metadata()))) {
    FuncIR* func = static_cast<FuncIR*>(ir_node);
    std::vector<std::string> other_args;
    for (ExpressionIR* arg : func->args()) {
      if (Match(arg, Metadata())) {
        continue;
      }
      other_args.push_back(arg->type_string());
    }
    return func->CreateIRNodeError(
        "Function '$0' with metadata arg in conjunction with '[$1]' is not supported.",
        func->func_name(), absl::StrJoin(other_args, ""));
  }
  return false;
}

StatusOr<MetadataLiteralIR*> MetadataFunctionFormatRule::WrapLiteral(
    DataIR* data, MetadataProperty* md_property) const {
  if (!md_property->ExprFitsFormat(data)) {
    return data->CreateIRNodeError("$0 not formatted properly for metadata operation. Expected $1",
                                   data->type_string(), md_property->ExplainFormat());
  }
  PL_ASSIGN_OR_RETURN(MetadataLiteralIR * literal,
                      data->graph_ptr()->MakeNode<MetadataLiteralIR>());
  PL_RETURN_IF_ERROR(literal->Init(data, data->ast_node()));

  return literal;
}

StatusOr<bool> CheckMetadataColumnNamingRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, MetadataResolver())) {
    // If the MetadataResolver, then don't do anything.
    return false;
  } else if (Match(ir_node, Map())) {
    return CheckMapColumns(static_cast<MapIR*>(ir_node));
  } else if (Match(ir_node, BlockingAgg())) {
    return CheckAggColumns(static_cast<BlockingAggIR*>(ir_node));
  }
  return false;
}  // namespace compiler

StatusOr<bool> CheckMetadataColumnNamingRule::CheckMapColumns(MapIR* op) const {
  for (const auto& col_expr : op->col_exprs()) {
    if (absl::StartsWith(col_expr.name, IdMetadataProperty::kMetadataColumnPrefix)) {
      return op->CreateIRNodeError(
          "Column name '$1' violates naming rules. The '$0' prefix is reserved for internal "
          "use.",
          MetadataProperty::kMetadataColumnPrefix, col_expr.name);
    }
  }
  return false;
}
StatusOr<bool> CheckMetadataColumnNamingRule::CheckAggColumns(BlockingAggIR* op) const {
  for (const auto& col_expr : op->aggregate_expressions()) {
    if (absl::StartsWith(col_expr.name, IdMetadataProperty::kMetadataColumnPrefix)) {
      return op->CreateIRNodeError(
          "Column name '$1' violates naming rules. The '$0' prefix is reserved for internal "
          "use.",
          MetadataProperty::kMetadataColumnPrefix, col_expr.name);
    }
  }
  return false;
}

StatusOr<bool> MetadataResolverConversionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, MetadataResolver())) {
    return ReplaceMetadataResolver(static_cast<MetadataResolverIR*>(ir_node));
  }
  return false;
}

Status MetadataResolverConversionRule::RemoveMetadataResolver(
    MetadataResolverIR* md_resolver) const {
  IR* graph = md_resolver->graph_ptr();
  // Get the parent of the metadsata resolver
  DCHECK_EQ(md_resolver->parents().size(), 1UL);
  OperatorIR* parent = md_resolver->parents()[0];
  PL_RETURN_IF_ERROR(md_resolver->RemoveParent(parent));

  // Get the child of the metadata operator.
  std::vector<int64_t> dependent_nodes = graph->dag().DependenciesOf(md_resolver->id());
  DCHECK_EQ(dependent_nodes.size(), 1UL);
  int64_t child_op = dependent_nodes[0];
  IRNode* node = graph->Get(child_op);
  DCHECK(node->IsOperator()) << "Expected node to be operator.";
  OperatorIR* op = static_cast<OperatorIR*>(node);

  // Set the parent of the child_op to the md_resolver parent.
  DCHECK(op->IsChildOf(md_resolver));
  PL_RETURN_IF_ERROR(op->ReplaceParent(md_resolver, parent));

  // delete metadata_resolver
  PL_RETURN_IF_ERROR(graph->DeleteNode(md_resolver->id()));
  return Status::OK();
}

Status MetadataResolverConversionRule::RemoveMap(MapIR* map) const {
  IR* graph = map->graph_ptr();
  DCHECK_EQ(map->parents().size(), 1UL);
  OperatorIR* parent_op = map->parents()[0];
  PL_RETURN_IF_ERROR(map->RemoveParent(parent_op));

  // Get the child of the metadata operator.
  std::vector<int64_t> map_dependent_nodes = graph->dag().DependenciesOf(map->id());
  CHECK_EQ(map_dependent_nodes.size(), map->col_exprs().size());
  for (const int64_t& child_node_idx : map_dependent_nodes) {
    PL_RETURN_IF_ERROR(graph->DeleteEdge(map->id(), child_node_idx));
    IRNode* node = graph->Get(child_node_idx);
    DCHECK(node->type() == IRNodeType::kColumn)
        << "Got: " << node->type_string() << "Expected: Column.";
    PL_RETURN_IF_ERROR(graph->DeleteNode(node->id()));
  }

  // Delete map.
  PL_RETURN_IF_ERROR(graph->DeleteNode(map->id()));
  return Status::OK();
}

bool MetadataResolverConversionRule::DoesMapOnlyCopy(MapIR* map) const {
  DCHECK_EQ(map->parents().size(), 1UL);
  Relation rel = map->parents()[0]->relation();
  if (rel.NumColumns() != map->col_exprs().size()) {
    return false;
  }
  int64_t idx = 0;
  for (auto const& col_expr : map->col_exprs()) {
    if (col_expr.node->type() != IRNodeType::kColumn && rel.GetColumnName(idx) != col_expr.name) {
      return false;
    }
    ++idx;
  }
  return true;
}
StatusOr<bool> MetadataResolverConversionRule::ReplaceMetadataResolver(
    MetadataResolverIR* md_resolver) const {
  PL_ASSIGN_OR_RETURN(MapIR * map, MakeMap(md_resolver));
  if (DoesMapOnlyCopy(map)) {
    PL_RETURN_IF_ERROR(RemoveMetadataResolver(md_resolver));
    PL_RETURN_IF_ERROR(RemoveMap(map));
    return true;
  }
  PL_RETURN_IF_ERROR(SwapInMap(md_resolver, map));
  return true;
}

Status MetadataResolverConversionRule::CopyParentColumns(IR* graph, OperatorIR* parent_op,
                                                         ColExpressionVector* col_exprs,
                                                         pypa::AstPtr ast_node) const {
  DCHECK(parent_op->IsRelationInit());
  Relation parent_relation = parent_op->relation();
  for (size_t i = 0; i < parent_relation.NumColumns(); ++i) {
    // Make Column
    PL_ASSIGN_OR_RETURN(ColumnIR * column_ir, graph->MakeNode<ColumnIR>());
    std::string column_name = parent_relation.GetColumnName(i);
    // Parent operator index is 1 because there is only 1 parent.
    PL_RETURN_IF_ERROR(column_ir->Init(column_name, /*parent_op_idx*/ 0, ast_node));
    column_ir->ResolveColumn(i, parent_relation.GetColumnType(i));
    col_exprs->emplace_back(column_name, column_ir);
  }
  return Status::OK();
}

Status MetadataResolverConversionRule::AddMetadataConversionFns(
    IR* graph, MetadataResolverIR* md_resolver, OperatorIR* parent_op,
    ColExpressionVector* col_exprs) const {
  Relation parent_relation = parent_op->relation();
  for (const auto& md_col_iter : md_resolver->metadata_columns()) {
    MetadataProperty* md_property = md_col_iter.second;
    // If parent relation has the column, we've already copied it, skip over.
    if (parent_relation.HasColumn(md_property->GetColumnRepr())) {
      continue;
    }
    PL_ASSIGN_OR_RETURN(FuncIR * conversion_func, graph->MakeNode<FuncIR>());
    PL_ASSIGN_OR_RETURN(std::string key_column,
                        FindKeyColumn(parent_relation, md_property, md_resolver));

    PL_ASSIGN_OR_RETURN(ColumnIR * column_ir, graph->MakeNode<ColumnIR>());
    // Parent op index is 0 because there is only one parent.
    PL_RETURN_IF_ERROR(column_ir->Init(key_column, /*parent_op_idx*/ 0, md_resolver->ast_node()));
    int64_t parent_relation_idx = parent_relation.GetColumnIndex(key_column);
    PL_ASSIGN_OR_RETURN(std::string func_name, md_property->UDFName(key_column));
    column_ir->ResolveColumn(parent_relation_idx, md_property->column_type());

    std::vector<types::DataType> children_data_types = {
        parent_relation.GetColumnType(parent_relation_idx)};
    PL_RETURN_IF_ERROR(conversion_func->Init({FuncIR::Opcode::non_op, "", func_name},
                                             ASTWalker::kRunTimeFuncPrefix, {column_ir}, false,
                                             md_resolver->ast_node()));
    PL_ASSIGN_OR_RETURN(types::DataType out_type,
                        compiler_state_->registry_info()->GetUDF(conversion_func->func_name(),
                                                                 children_data_types));
    conversion_func->set_func_id(
        compiler_state_->GetUDFID(RegistryKey(conversion_func->func_name(), children_data_types)));

    // Conversion Func.
    DCHECK_EQ(out_type, md_property->column_type())
        << "Expected the parent_relation key column type and metadata property type to match.";

    conversion_func->SetOutputDataType(out_type);
    conversion_func->SetArgsTypes(children_data_types);
    col_exprs->emplace_back(md_property->GetColumnRepr(), conversion_func);
  }
  return Status::OK();
}

StatusOr<MapIR*> MetadataResolverConversionRule::MakeMap(MetadataResolverIR* md_resolver) const {
  IR* graph = md_resolver->graph_ptr();
  DCHECK_EQ(md_resolver->parents().size(), 1UL);
  OperatorIR* parent_op = md_resolver->parents()[0];
  ColExpressionVector col_exprs;
  PL_RETURN_IF_ERROR(CopyParentColumns(graph, parent_op, &col_exprs, md_resolver->ast_node()));

  PL_RETURN_IF_ERROR(AddMetadataConversionFns(graph, md_resolver, parent_op, &col_exprs));
  Relation relation = md_resolver->relation();
  std::unordered_set<std::string> col_names(std::make_move_iterator(relation.col_names().begin()),
                                            std::make_move_iterator(relation.col_names().end()));
  DCHECK_EQ(col_exprs.size(), md_resolver->relation().NumColumns());
  PL_ASSIGN_OR_RETURN(MapIR * map, graph->MakeNode<MapIR>());
  PL_ASSIGN_OR_RETURN(LambdaIR * lambda, graph->MakeNode<LambdaIR>());
  PL_RETURN_IF_ERROR(lambda->Init(col_names, col_exprs, md_resolver->ast_node()));
  PL_RETURN_IF_ERROR(map->Init(parent_op, {{"fn", lambda}}, md_resolver->ast_node()));
  return map;
}

StatusOr<std::string> MetadataResolverConversionRule::FindKeyColumn(const Relation& parent_relation,
                                                                    MetadataProperty* property,
                                                                    IRNode* node_for_error) const {
  for (const std::string& key_col : property->GetKeyColumnReprs()) {
    if (parent_relation.HasColumn(key_col)) {
      return key_col;
    }
  }
  return node_for_error->CreateIRNodeError(
      "Can't resolve metadata because of lack of converting columns in the parent. Need one of "
      "[$0]. Parent relation has columns [$1] available.",
      absl::StrJoin(property->GetKeyColumnReprs(), ","),
      absl::StrJoin(parent_relation.col_names(), ","));
}
Status MetadataResolverConversionRule::SwapInMap(MetadataResolverIR* md_resolver,
                                                 MapIR* map) const {
  IR* graph = md_resolver->graph_ptr();
  DCHECK_EQ(md_resolver->parents().size(), 1UL);

  PL_RETURN_IF_ERROR(md_resolver->RemoveParent(md_resolver->parents()[0]));
  std::vector<int64_t> dependent_nodes = graph->dag().DependenciesOf(md_resolver->id());
  DCHECK_EQ(dependent_nodes.size(), 1UL);

  IRNode* node = graph->Get(dependent_nodes[0]);
  DCHECK(node->IsOperator()) << "Expected node to be operator.";
  OperatorIR* op = static_cast<OperatorIR*>(node);

  DCHECK(op->IsChildOf(md_resolver));
  DCHECK(md_resolver->IsRelationInit());

  PL_RETURN_IF_ERROR(op->ReplaceParent(md_resolver, map));
  PL_RETURN_IF_ERROR(map->SetRelation(md_resolver->relation()));

  // delete metadata_resolver
  PL_RETURN_IF_ERROR(graph->DeleteNode(md_resolver->id()));
  return Status::OK();
}

StatusOr<bool> MergeRangeOperatorRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, Range())) {
    return MergeRange(static_cast<RangeIR*>(ir_node));
  }
  return false;
}
StatusOr<bool> MergeRangeOperatorRule::MergeRange(RangeIR* range_ir) {
  IR* ir_graph = range_ir->graph_ptr();
  DCHECK_EQ(range_ir->parents().size(), 1UL);
  OperatorIR* range_parent = range_ir->parents()[0];

  if (range_parent->type() != IRNodeType::kMemorySource) {
    return range_parent->CreateIRNodeError("Expected range parent to be a MemorySource, not a $0.",
                                           range_parent->type_string());
  }

  MemorySourceIR* src_ir = static_cast<MemorySourceIR*>(range_parent);
  IntIR* start_time_ir = static_cast<IntIR*>(range_ir->start_repr());
  IntIR* stop_time_ir = static_cast<IntIR*>(range_ir->stop_repr());

  src_ir->SetTime(start_time_ir->val(), stop_time_ir->val());

  DeferNodeDeletion(start_time_ir->id());
  DeferNodeDeletion(stop_time_ir->id());

  // Update all of range's dependencies to point to src.
  for (const auto& dep_id : ir_graph->dag().DependenciesOf(range_ir->id())) {
    auto dep = ir_graph->Get(dep_id);
    if (!dep->IsOperator()) {
      PL_RETURN_IF_ERROR(ir_graph->DeleteEdge(range_ir->id(), dep_id));
      PL_RETURN_IF_ERROR(ir_graph->AddEdge(src_ir->id(), dep_id));
      continue;
    }
    auto casted_node = static_cast<OperatorIR*>(dep);
    PL_RETURN_IF_ERROR(casted_node->ReplaceParent(range_ir, src_ir));
  }
  PL_RETURN_IF_ERROR(range_ir->RemoveParent(src_ir));
  DeferNodeDeletion(range_ir->id());
  return true;
}

StatusOr<bool> JoinEqualityConditionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, JoinOperatorEqCondNotSet())) {
    return SetEqualityConditionJoin(static_cast<JoinIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> JoinEqualityConditionRule::SetEqualityConditionJoin(JoinIR* join_node) {
  PL_RETURN_IF_ERROR(ProcessExpression(join_node, join_node->condition_expr()));
  return true;
}

Status JoinEqualityConditionRule::ProcessExpression(JoinIR* join_node, ExpressionIR* expr) {
  if (Match(expr, Equals(ColumnNode(), ColumnNode()))) {
    FuncIR* eq_func = static_cast<FuncIR*>(expr);
    DCHECK_EQ(eq_func->args().size(), 2UL);
    ColumnIR* arg0 = static_cast<ColumnIR*>(eq_func->args()[0]);
    ColumnIR* arg1 = static_cast<ColumnIR*>(eq_func->args()[1]);

    int64_t arg0_parent_idx = arg0->container_op_parent_idx();
    int64_t arg1_parent_idx = arg1->container_op_parent_idx();

    if (arg0_parent_idx == arg1_parent_idx) {
      return expr->CreateIRNodeError(
          "Both sides of an equality condition can't reference the same parent, you must reference "
          "the other parent.",
          arg0_parent_idx);
    }

    int64_t arg0_idx = arg0->col_idx();
    int64_t arg1_idx = arg1->col_idx();

    if (arg0_parent_idx == 0 && arg1_parent_idx == 1) {
      join_node->AddEqualityCondition(arg0_idx, arg1_idx);
    } else if (arg0_parent_idx == 1 && arg1_parent_idx == 0) {
      join_node->AddEqualityCondition(arg1_idx, arg0_idx);
    } else {
      return expr->CreateIRNodeError("Parents of columns in equality condition not set properly.");
    }
    return Status::OK();
  } else if (Match(expr, LogicalAnd())) {
    FuncIR* func = static_cast<FuncIR*>(expr);
    DCHECK_EQ(func->args().size(), 2UL);
    PL_RETURN_IF_ERROR(ProcessExpression(join_node, func->args()[0]));
    PL_RETURN_IF_ERROR(ProcessExpression(join_node, func->args()[1]));
    return Status::OK();
  }

  return expr->CreateIRNodeError(
      "Expression $0 not supported as a condition in the Join. Only equality of columns and the "
      "AND compositions of equality expressions are supported.",
      expr->DebugString());
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
