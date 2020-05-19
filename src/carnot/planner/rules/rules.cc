#include "src/carnot/planner/rules/rules.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <absl/container/flat_hash_set.h>

namespace pl {
namespace carnot {
namespace planner {

using table_store::schema::Relation;

StatusOr<bool> DataTypeRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedRTFuncMatchAllArgs(ResolvedExpression()))) {
    // Match any function that has all args resolved.
    return EvaluateFunc(compiler_state_, static_cast<FuncIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedColumnType())) {
    // Evaluate any unresolved columns.
    return EvaluateColumn(static_cast<ColumnIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedMetadataType())) {
    // Evaluate any unresolved columns.
    return EvaluateMetadata(static_cast<MetadataIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> DataTypeRule::EvaluateFunc(CompilerState* compiler_state, FuncIR* func) {
  // Get the types of the children of this function.
  std::vector<types::DataType> children_data_types;
  for (const auto& arg : func->args()) {
    types::DataType t = arg->EvaluatedDataType();
    DCHECK(t != types::DataType::DATA_TYPE_UNKNOWN);
    children_data_types.push_back(t);
  }

  auto udftype_or_s = compiler_state->registry_info()->GetUDFType(func->func_name());
  if (!udftype_or_s.ok()) {
    return func->CreateIRNodeError(udftype_or_s.status().msg());
  }
  switch (udftype_or_s.ConsumeValueOrDie()) {
    case UDFType::kUDF: {
      auto data_type_or_s =
          compiler_state->registry_info()->GetUDF(func->func_name(), children_data_types);
      if (!data_type_or_s.status().ok()) {
        return func->CreateIRNodeError(data_type_or_s.status().msg());
      }
      types::DataType data_type = data_type_or_s.ConsumeValueOrDie();
      func->set_func_id(
          compiler_state->GetUDFID(RegistryKey(func->func_name(), children_data_types)));
      func->SetOutputDataType(data_type);
      break;
    }
    case UDFType::kUDA: {
      PL_ASSIGN_OR_RETURN(types::DataType data_type, compiler_state->registry_info()->GetUDA(
                                                         func->func_name(), children_data_types));
      func->set_func_id(
          compiler_state->GetUDAID(RegistryKey(func->func_name(), children_data_types)));
      func->SetOutputDataType(data_type);
      break;
    }
    default: {
      return error::Internal("Unsupported UDFType");
    }
  }

  func->SetArgsTypes(children_data_types);
  return true;
}

StatusOr<bool> DataTypeRule::EvaluateColumn(ColumnIR* column) {
  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op, column->ReferencedOperator());
  if (!parent_op->IsRelationInit()) {
    // Missing a relation in parent op is not a failure - the parent op still has to
    // propagate results.
    return false;
  }

  // Get the parent relation and find the column in it.
  PL_RETURN_IF_ERROR(EvaluateColumnFromRelation(column, parent_op->relation()));
  return true;
}

StatusOr<bool> DataTypeRule::EvaluateMetadata(MetadataIR* md) {
  md->ResolveColumnType(md->property()->column_type());
  return true;
}

Status DataTypeRule::EvaluateColumnFromRelation(ColumnIR* column, const Relation& relation) {
  if (!relation.HasColumn(column->col_name())) {
    return column->CreateIRNodeError("Column '$0' not found in parent dataframe",
                                     column->col_name());
  }
  column->ResolveColumnType(relation);
  return Status::OK();
}

StatusOr<bool> SourceRelationRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedSource())) {
    return GetSourceRelation(static_cast<OperatorIR*>(ir_node));
  }
  return false;
}

std::vector<int64_t> SourceRelationRule::GetColumnIndexMap(
    const std::vector<std::string>& col_names, const Relation& relation) const {
  auto result = std::vector<int64_t>();
  // Finds the index of each column and pushes to the out vector.
  for (const auto& col_name : col_names) {
    result.push_back(relation.GetColumnIndex(col_name));
  }
  return result;
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

  // Get the children.
  std::vector<std::string> columns;
  Relation select_relation;
  if (!mem_node->select_all()) {
    columns = mem_node->column_names();
    PL_ASSIGN_OR_RETURN(select_relation, GetSelectRelation(mem_node, table_relation, columns));
  } else {
    columns = table_relation.col_names();
    select_relation = table_relation;
  }
  mem_node->SetColumnIndexMap(GetColumnIndexMap(columns, table_relation));
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
    if (!relation.HasColumn(col_name)) {
      return node->CreateIRNodeError("Column '$0' not found in parent dataframe", col_name);
    }
    PL_ASSIGN_OR_RETURN(auto col_node, graph->CreateNode<ColumnIR>(node->ast_node(), col_name,
                                                                   /*parent_op_idx*/ 0));
    col_node->ResolveColumnType(relation);
    result.push_back(col_node);
  }
  return result;
}

StatusOr<bool> OperatorRelationRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedReadyOp(BlockingAgg()))) {
    return SetBlockingAgg(static_cast<BlockingAggIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyOp(Map()))) {
    return SetMap(static_cast<MapIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyOp(MetadataResolver()))) {
    return SetMetadataResolver(static_cast<MetadataResolverIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyOp(Union()))) {
    return SetUnion(static_cast<UnionIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyOp(Join()))) {
    JoinIR* join_node = static_cast<JoinIR*>(ir_node);
    if (Match(ir_node, UnsetOutputColumnsJoin())) {
      PL_RETURN_IF_ERROR(SetJoinOutputColumns(join_node));
    }
    return SetOldJoin(join_node);
  } else if (Match(ir_node, UnresolvedReadyOp(Drop()))) {
    // Another rule handles this.
    // TODO(philkuz) unify this rule with the drop to map rule.
    return false;
  } else if (Match(ir_node, UnresolvedReadyOp(MemorySink()))) {
    return SetMemorySink(static_cast<MemorySinkIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyOp(Limit())) ||
             Match(ir_node, UnresolvedReadyOp(Filter())) ||
             Match(ir_node, UnresolvedReadyOp(GroupBy())) ||
             Match(ir_node, UnresolvedReadyOp(Rolling()))) {
    // Explicitly match because the general matcher keeps causing problems.
    return SetOther(static_cast<OperatorIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedReadyOp())) {
    // Fails in this path because future writers should specify the op.
    DCHECK(false) << ir_node->DebugString();
    return SetOther(static_cast<OperatorIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> OperatorRelationRule::SetOldJoin(JoinIR* join_node) const {
  DCHECK_EQ(join_node->parents().size(), 2UL);
  OperatorIR* left = join_node->parents()[0];
  OperatorIR* right = join_node->parents()[1];

  Relation left_relation = left->relation();
  Relation right_relation = right->relation();

  Relation out_relation;

  for (const auto& [col_idx, col] : Enumerate(join_node->output_columns())) {
    if (!col->IsDataTypeEvaluated()) {
      return false;
    }
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

StatusOr<ColumnIR*> OperatorRelationRule::CreateOutputColumn(JoinIR* join_node,
                                                             const std::string& col_name,
                                                             int64_t parent_idx,
                                                             const Relation& relation) const {
  PL_ASSIGN_OR_RETURN(ColumnIR * col, join_node->graph_ptr()->CreateNode<ColumnIR>(
                                          join_node->ast_node(), col_name, parent_idx));
  PL_RETURN_IF_ERROR(DataTypeRule::EvaluateColumnFromRelation(col, relation));
  return col;
}

StatusOr<std::vector<ColumnIR*>> OperatorRelationRule::CreateOutputColumnIRNodes(
    JoinIR* join_node, const Relation& left_relation, const Relation& right_relation) const {
  int64_t left_idx = 0;
  int64_t right_idx = 1;
  if (join_node->specified_as_right()) {
    left_idx = 1;
    right_idx = 0;
  }
  std::vector<ColumnIR*> output_columns;
  output_columns.reserve(left_relation.NumColumns() + right_relation.NumColumns());
  for (const auto& col_name : left_relation.col_names()) {
    PL_ASSIGN_OR_RETURN(
        ColumnIR * col,
        CreateOutputColumn(join_node, col_name, /* parent_idx */ left_idx, left_relation));
    output_columns.push_back(col);
  }
  for (const auto& right_df_name : right_relation.col_names()) {
    PL_ASSIGN_OR_RETURN(ColumnIR * col,
                        CreateOutputColumn(join_node, right_df_name,
                                           /* parent_idx */ right_idx, right_relation));
    output_columns.push_back(col);
  }
  return output_columns;
}

Status OperatorRelationRule::SetJoinOutputColumns(JoinIR* join_node) const {
  DCHECK_EQ(join_node->parents().size(), 2UL);
  int64_t left_idx = 0;
  int64_t right_idx = 1;

  if (join_node->specified_as_right()) {
    left_idx = 1;
    right_idx = 0;
  }
  const Relation& left_relation = join_node->parents()[left_idx]->relation();
  const Relation& right_relation = join_node->parents()[right_idx]->relation();
  const std::string& left_suffix = join_node->suffix_strs()[left_idx];
  const std::string& right_suffix = join_node->suffix_strs()[right_idx];

  absl::flat_hash_set<std::string> columns_set(left_relation.col_names().begin(),
                                               left_relation.col_names().end());
  columns_set.reserve(left_relation.NumColumns() + right_relation.NumColumns());
  // The left relation should only have unique names.
  DCHECK_EQ(columns_set.size(), left_relation.NumColumns())
      << "Left relation has duplicate columns, should have caught this earlier.";

  std::vector<std::string> output_column_names(left_relation.col_names().begin(),
                                               left_relation.col_names().end());
  output_column_names.reserve(left_relation.NumColumns() + right_relation.NumColumns());

  absl::flat_hash_set<std::string> duplicate_column_names;
  for (const auto& right_df_name : right_relation.col_names()) {
    // Output columns are added to regardless, we replace both duplicated columns in the following
    // loop.
    output_column_names.push_back(right_df_name);

    if (columns_set.contains(right_df_name)) {
      duplicate_column_names.insert(right_df_name);
      continue;
    }
    columns_set.insert(right_df_name);
  }

  // Resolve any of the duplicates, check to see if there are duplicates afterwards
  for (const auto& dup_name : duplicate_column_names) {
    columns_set.erase(dup_name);
    std::string left_column = absl::Substitute("$0$1", dup_name, left_suffix);
    std::string right_column = absl::Substitute("$0$1", dup_name, right_suffix);

    std::string err_string = absl::Substitute(
        "duplicate column '$0' after merge. Change the specified suffixes ('$1','$2') to fix "
        "this",
        "$0", left_suffix, right_suffix);
    // Make sure that the new left_column doesn't already exist in the
    if (columns_set.contains(left_column)) {
      return join_node->CreateIRNodeError(err_string, left_column);
    }
    // Insert before checking right column to make sure left_column != right_column. Saves a check.
    columns_set.insert(left_column);
    if (columns_set.contains(right_column)) {
      return join_node->CreateIRNodeError(err_string, right_column);
    }
    columns_set.insert(right_column);
    ReplaceDuplicateNames(&output_column_names, dup_name, left_column, right_column);
  }

  // Assertion that columns are the same size as the sum of the columns.
  DCHECK_EQ(columns_set.size(), left_relation.NumColumns() + right_relation.NumColumns());
  PL_ASSIGN_OR_RETURN(auto output_columns,
                      CreateOutputColumnIRNodes(join_node, left_relation, right_relation));

  return join_node->SetOutputColumns(output_column_names, output_columns);
}

void OperatorRelationRule::ReplaceDuplicateNames(std::vector<std::string>* column_names,
                                                 const std::string& dup_name,
                                                 const std::string& left_column,
                                                 const std::string& right_column) const {
  bool left_found = false;
  bool right_found = false;
  // Iterate through the output column names and replace the two duplicate columns.
  for (const auto& [idx, str] : Enumerate(*column_names)) {
    if (str != dup_name) {
      continue;
    }
    // Left column should be found first.
    if (left_found) {
      (*column_names)[idx] = right_column;
      // When right column is found, then we exit the loop.
      right_found = true;
      break;
    }
    (*column_names)[idx] = left_column;
    left_found = true;
  }
  DCHECK(right_found);
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

Relation RelationFromExprs(const ColExpressionVector& exprs) {
  Relation rel;
  // Make a new relation with each of the expression key, type pairs.
  for (auto& entry : exprs) {
    DCHECK(entry.node->IsDataTypeEvaluated());
    rel.AddColumn(entry.node->EvaluatedDataType(), entry.name);
  }
  return rel;
}

StatusOr<bool> OperatorRelationRule::SetMap(MapIR* map_ir) const {
  DCHECK_EQ(map_ir->parents().size(), 1UL) << "There should be exactly one parent.";
  auto parent_relation = map_ir->parents()[0]->relation();
  const ColExpressionVector& expressions = map_ir->col_exprs();

  for (auto& entry : expressions) {
    if (!entry.node->IsDataTypeEvaluated()) {
      return false;
    }
  }

  if (map_ir->keep_input_columns()) {
    ColExpressionVector output_expressions;

    absl::flat_hash_set<std::string> new_columns;
    for (ColumnExpression expr : expressions) {
      new_columns.insert(expr.name);
    }

    for (const auto& input_col_name : parent_relation.col_names()) {
      // If this column is being overwritten with a new expression, skip it here.
      if (new_columns.contains(input_col_name)) {
        continue;
      }
      // Otherwise, bring over the column from the previous relation.
      PL_ASSIGN_OR_RETURN(ColumnIR * col_ir,
                          map_ir->graph_ptr()->CreateNode<ColumnIR>(
                              map_ir->ast_node(), input_col_name, 0 /*parent_op_idx*/));
      col_ir->ResolveColumnType(parent_relation);
      output_expressions.push_back(ColumnExpression(input_col_name, col_ir));
    }

    for (const ColumnExpression& expr : expressions) {
      output_expressions.push_back(expr);
    }

    map_ir->set_keep_input_columns(false);
    PL_RETURN_IF_ERROR(map_ir->SetColExprs(output_expressions));
  }

  PL_RETURN_IF_ERROR(map_ir->SetRelation(RelationFromExprs(map_ir->col_exprs())));
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

StatusOr<bool> OperatorRelationRule::SetMemorySink(MemorySinkIR* sink_ir) const {
  if (!sink_ir->out_columns().size()) {
    return SetOther(sink_ir);
  }
  auto input_relation = sink_ir->parents()[0]->relation();
  Relation output_relation;
  for (const auto& col_name : sink_ir->out_columns()) {
    output_relation.AddColumn(input_relation.GetColumnType(col_name), col_name,
                              input_relation.GetColumnDesc(col_name));
  }
  PL_RETURN_IF_ERROR(sink_ir->SetRelation(output_relation));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetOther(OperatorIR* operator_ir) const {
  CHECK_EQ(operator_ir->parents().size(), 1UL);
  PL_RETURN_IF_ERROR(operator_ir->SetRelation(operator_ir->parents()[0]->relation()));
  return true;
}

StatusOr<ExpressionIR*> EvaluateCompileTimeExpr::Evaluate(ExpressionIR* ir_node) {
  if (!Match(ir_node, Func())) {
    return ir_node;
  }

  auto func_ir = static_cast<FuncIR*>(ir_node);

  for (const auto& [i, arg] : Enumerate(func_ir->args())) {
    PL_ASSIGN_OR_RETURN(auto new_arg, Evaluate(arg));
    PL_RETURN_IF_ERROR(func_ir->UpdateArg(i, new_arg));
  }

  if (Match(func_ir, CompileTimeIntegerArithmetic())) {
    return EvalArithmetic(func_ir);
  }
  if (Match(func_ir, CompileTimeNow())) {
    return EvalTimeNow(func_ir);
  }
  if (Match(func_ir, CompileTimeUnitTime())) {
    return EvalUnitTime(func_ir);
  }

  if (Match(func_ir, CompileTimeFunc())) {
    return ir_node->CreateIRNodeError(
        "Node is a compile time func but it did not match any known compile time funcs");
  }

  return func_ir;
}

StatusOr<IntIR*> EvaluateCompileTimeExpr::EvalArithmetic(FuncIR* func_ir) {
  if (func_ir->args().size() != 2) {
    return func_ir->CreateIRNodeError("Expected 2 argument to $0 call, got $1.",
                                      func_ir->carnot_op_name(), func_ir->args().size());
  }

  std::vector<IntIR*> casted;
  for (const auto& arg : func_ir->args()) {
    if (arg->type() != IRNodeType::kInt) {
      return func_ir->CreateIRNodeError("Expected integer arguments only to function $0",
                                        func_ir->carnot_op_name());
    }
    casted.push_back(static_cast<IntIR*>(arg));
  }

  int64_t result = 0;
  // TODO(philkuz) (PL-709) Make a UDCF (C := CompileTime) to combine these together.
  if (func_ir->opcode() == FuncIR::Opcode::mult) {
    result = 1;
    for (auto a : casted) {
      result *= a->val();
    }
  } else if (func_ir->opcode() == FuncIR::Opcode::add) {
    for (auto a : casted) {
      result += a->val();
    }
  } else if (func_ir->opcode() == FuncIR::Opcode::sub) {
    result = casted[0]->val() - casted[1]->val();
  } else {
    return func_ir->CreateIRNodeError("Only allowing [multiply, add, subtract], not $0",
                                      func_ir->carnot_op_name());
  }

  return func_ir->graph_ptr()->CreateNode<IntIR>(func_ir->ast_node(), result);
}

StatusOr<IntIR*> EvaluateCompileTimeExpr::EvalTimeNow(FuncIR* func_ir) {
  CHECK_EQ(func_ir->args().size(), 0U)
      << "Received unexpected args for " << func_ir->carnot_op_name() << " function";
  return func_ir->graph_ptr()->CreateNode<IntIR>(func_ir->ast_node(),
                                                 compiler_state_->time_now().val);
}

StatusOr<IntIR*> EvaluateCompileTimeExpr::EvalUnitTime(FuncIR* func_ir) {
  CHECK_EQ(func_ir->args().size(), 1U)
      << "Expected exactly 1 arg for " << func_ir->carnot_op_name() << " function";
  auto fn_type_iter = kUnitTimeFnStr.find(func_ir->carnot_op_name());
  if (fn_type_iter == kUnitTimeFnStr.end()) {
    return func_ir->CreateIRNodeError("Time unit function '$0' not found",
                                      func_ir->carnot_op_name());
  }

  auto arg = func_ir->args()[0];
  if (!Match(arg, Int())) {
    return func_ir->CreateIRNodeError("Expected integer for argument in ",
                                      func_ir->carnot_op_name());
  }
  int64_t time_val = static_cast<IntIR*>(arg)->val();

  std::chrono::nanoseconds time_output;
  auto time_unit = fn_type_iter->second;
  time_output = time_unit * time_val;
  // create the ir_node;
  return func_ir->graph_ptr()->CreateNode<IntIR>(func_ir->ast_node(), time_output.count());
}

StatusOr<bool> OperatorCompileTimeExpressionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, Map())) {
    return EvalMap(static_cast<MapIR*>(ir_node));
  } else if (Match(ir_node, Filter())) {
    return EvalFilter(static_cast<FilterIR*>(ir_node));
  } else if (Match(ir_node, MemorySource())) {
    return EvalMemorySource(static_cast<MemorySourceIR*>(ir_node));
  } else if (Match(ir_node, Rolling())) {
    return EvalRolling(static_cast<RollingIR*>(ir_node));
  } else if (Match(ir_node, Limit())) {
    // TODO(nserrino, philkuz): (PL-1161) Add support for compile time evaluation of Limit argument.
    return false;
  }
  return false;
}

StatusOr<ExpressionIR*> OperatorCompileTimeExpressionRule::EvalCompileTimeSubExpressions(
    ExpressionIR* expr) {
  if (!Match(expr, ContainsCompileTimeFunc())) {
    return expr;
  }

  if (Match(expr, CompileTimeFunc())) {
    EvaluateCompileTimeExpr evaluator(compiler_state_);
    PL_ASSIGN_OR_RETURN(auto evaled, evaluator.Evaluate(expr));
    return evaled;
  }

  auto func = static_cast<FuncIR*>(expr);
  std::vector<ExpressionIR*> evaled_args;

  for (const auto& [idx, arg] : Enumerate(func->args())) {
    if (!Match(arg, ContainsCompileTimeFunc())) {
      continue;
    }
    PL_ASSIGN_OR_RETURN(auto evaled, EvalCompileTimeSubExpressions(arg));
    PL_RETURN_IF_ERROR(func->UpdateArg(idx, evaled));
  }

  return func;
}

StatusOr<bool> OperatorCompileTimeExpressionRule::EvalMap(MapIR* ir_node) {
  bool evaled = false;

  ColExpressionVector exprs;
  for (const auto& expr : ir_node->col_exprs()) {
    if (!Match(expr.node, ContainsCompileTimeFunc())) {
      exprs.push_back(expr);
      continue;
    }
    evaled = true;
    PL_ASSIGN_OR_RETURN(auto new_expr, EvalCompileTimeSubExpressions(expr.node));
    exprs.emplace_back(expr.name, new_expr);
  }
  if (evaled) {
    PL_RETURN_IF_ERROR(ir_node->SetColExprs(exprs));
  }
  return evaled;
}

StatusOr<bool> OperatorCompileTimeExpressionRule::EvalFilter(FilterIR* ir_node) {
  if (!Match(ir_node->filter_expr(), ContainsCompileTimeFunc())) {
    return false;
  }
  PL_ASSIGN_OR_RETURN(auto new_expr, EvalCompileTimeSubExpressions(ir_node->filter_expr()));
  PL_RETURN_IF_ERROR(ir_node->SetFilterExpr(new_expr));
  return true;
}

StatusOr<bool> OperatorCompileTimeExpressionRule::EvalMemorySource(MemorySourceIR* mem_src) {
  if (mem_src->IsTimeSet() || !mem_src->HasTimeExpressions()) {
    return false;
  }

  PL_ASSIGN_OR_RETURN(auto start, EvalCompileTimeSubExpressions(mem_src->start_time_expr()));
  PL_ASSIGN_OR_RETURN(auto stop, EvalCompileTimeSubExpressions(mem_src->end_time_expr()));
  PL_RETURN_IF_ERROR(mem_src->SetTimeExpressions(start, stop));
  return true;
}

StatusOr<bool> OperatorCompileTimeExpressionRule::EvalRolling(RollingIR* rolling) {
  PL_ASSIGN_OR_RETURN(auto new_window_size, EvalCompileTimeSubExpressions(rolling->window_size()));
  auto changed = new_window_size->id() != rolling->window_size()->id();
  PL_RETURN_IF_ERROR(rolling->ReplaceWindowSize(new_window_size));
  return changed;
}

// This feels like the wrong way to be handling String->Time Conversion,
// should consider rearchitecting it
StatusOr<bool> ConvertStringTimesRule::Apply(IRNode* node) {
  if (Match(node, MemorySource())) {
    MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(node);
    return HandleMemSrc(mem_src);
  } else if (Match(node, Rolling())) {
    RollingIR* rolling = static_cast<RollingIR*>(node);
    return HandleRolling(rolling);
  } else {
    return false;
  }
}

StatusOr<bool> ConvertStringTimesRule::HandleMemSrc(MemorySourceIR* mem_src) {
  if (mem_src->IsTimeSet() || !mem_src->HasTimeExpressions()) {
    return false;
  }
  bool start_has_string_time = HasStringTime(mem_src->start_time_expr());
  bool end_has_string_time = HasStringTime(mem_src->end_time_expr());

  if (!start_has_string_time && !end_has_string_time) {
    return false;
  }

  ExpressionIR* start_time = mem_src->start_time_expr();
  ExpressionIR* end_time = mem_src->end_time_expr();

  if (start_has_string_time) {
    PL_ASSIGN_OR_RETURN(start_time, ConvertStringTimes(start_time, /* relative_time */ true));
  }
  if (end_has_string_time) {
    PL_ASSIGN_OR_RETURN(end_time, ConvertStringTimes(end_time, /* relative_time */ true));
  }

  PL_RETURN_IF_ERROR(mem_src->SetTimeExpressions(start_time, end_time));
  return true;
}

StatusOr<bool> ConvertStringTimesRule::HandleRolling(RollingIR* rolling) {
  if (!HasStringTime(rolling->window_size())) {
    return false;
  }
  PL_ASSIGN_OR_RETURN(ExpressionIR * new_window_size,
                      ConvertStringTimes(rolling->window_size(), /* relative_time */ false));
  PL_RETURN_IF_ERROR(rolling->ReplaceWindowSize(new_window_size));
  return true;
}

bool ConvertStringTimesRule::HasStringTime(const ExpressionIR* node) {
  if (Match(node, String())) {
    return true;
  }
  if (Match(node, Func())) {
    const FuncIR* func = static_cast<const FuncIR*>(node);
    for (const ExpressionIR* arg : func->args()) {
      bool has_string_time = HasStringTime(arg);
      if (has_string_time) {
        return true;
      }
    }
  }
  return false;
}

// Support taking strings like "-2m" into a memory source or rolling operator.
// relative_time determines whether to add in the current compiler time or just
// use the time given by the string
// TODO(nserrino, philkuz) Figure out if we can generalize so that it can work in other operators
// without polluting our approach to types.
StatusOr<ExpressionIR*> ConvertStringTimesRule::ConvertStringTimes(ExpressionIR* node,
                                                                   bool relative_time) {
  // Mem sources treat expressions differently than other nodes, so if we run into one with
  // a shared parent, we should clone it to make sure that that operator doesn't get the same
  // special case treatment of the expression.
  if (node->graph_ptr()->dag().ParentsOf(node->id()).size() > 1) {
    PL_ASSIGN_OR_RETURN(IRNode * copy, node->graph_ptr()->CopyNode(node));
    CHECK(Match(copy, Expression()));
    node = static_cast<ExpressionIR*>(copy);
  }

  if (Match(node, String())) {
    auto str_node = static_cast<StringIR*>(node);
    PL_ASSIGN_OR_RETURN(int64_t int_val, StringToTimeInt(str_node->str()));
    int64_t time_repr = int_val;
    if (relative_time) {
      time_repr += compiler_state_->time_now().val;
    }
    PL_ASSIGN_OR_RETURN(auto out_node,
                        node->graph_ptr()->CreateNode<IntIR>(node->ast_node(), time_repr));
    return out_node;
  } else if (Match(node, Func())) {
    auto func_node = static_cast<FuncIR*>(node);
    for (const auto& [idx, arg] : Enumerate(func_node->args())) {
      PL_ASSIGN_OR_RETURN(auto eval_result, ConvertStringTimes(arg, relative_time));
      if (eval_result != arg) {
        PL_RETURN_IF_ERROR(func_node->UpdateArg(idx, eval_result));
      }
    }
    return func_node;
  }
  return node;
}

StatusOr<bool> SetMemSourceNsTimesRule::Apply(IRNode* node) {
  if (!Match(node, MemorySource())) {
    return false;
  }
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(node);
  if (mem_src->IsTimeSet() || !mem_src->HasTimeExpressions()) {
    return false;
  }

  DCHECK(Match(mem_src->start_time_expr(), Int()));
  DCHECK(Match(mem_src->end_time_expr(), Int()));
  mem_src->SetTimeValuesNS(static_cast<IntIR*>(mem_src->start_time_expr())->val(),
                           static_cast<IntIR*>(mem_src->end_time_expr())->val());
  return true;
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
  // Metadata Resolver is now child of Parent.
  PL_ASSIGN_OR_RETURN(MetadataResolverIR * md_resolver,
                      graph->CreateNode<MetadataResolverIR>(container_op->ast_node(), parent_op));
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
  PL_ASSIGN_OR_RETURN(std::vector<OperatorIR*> container_ops, metadata->ContainingOperators());
  DCHECK_EQ(container_ops.size(), 1UL);
  OperatorIR* container_op = container_ops[0];
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
    std::string column_name = parent_relation.GetColumnName(i);
    // Make Column
    // Parent operator index is 0 because there is only 1 parent.
    PL_ASSIGN_OR_RETURN(ColumnIR * column_ir,
                        graph->CreateNode<ColumnIR>(ast_node, column_name, /*parent_op_idx*/ 0));
    column_ir->ResolveColumnType(parent_relation);
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
    PL_ASSIGN_OR_RETURN(std::string key_column,
                        FindKeyColumn(parent_relation, md_property, md_resolver));

    // Parent op index is 0 because there is only one parent.
    PL_ASSIGN_OR_RETURN(
        ColumnIR * column_ir,
        graph->CreateNode<ColumnIR>(md_resolver->ast_node(), key_column, /*parent_op_idx*/ 0));
    PL_ASSIGN_OR_RETURN(std::string func_name, md_property->UDFName(key_column));
    column_ir->ResolveColumnType(md_property->column_type());

    std::vector<types::DataType> children_data_types = {parent_relation.GetColumnType(key_column)};
    PL_ASSIGN_OR_RETURN(FuncIR * conversion_func,
                        graph->CreateNode<FuncIR>(md_resolver->ast_node(),
                                                  FuncIR::Op{FuncIR::Opcode::non_op, "", func_name},
                                                  std::vector<ExpressionIR*>{column_ir}));
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
  PL_ASSIGN_OR_RETURN(MapIR * map,
                      graph->CreateNode<MapIR>(md_resolver->ast_node(), parent_op, col_exprs,
                                               /* keep_input_columns */ false));
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

StatusOr<bool> DropToMapOperatorRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedReadyOp(Drop()))) {
    return DropToMap(static_cast<DropIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> DropToMapOperatorRule::DropToMap(DropIR* drop_ir) {
  IR* ir_graph = drop_ir->graph_ptr();
  DCHECK_EQ(drop_ir->parents().size(), 1UL);
  OperatorIR* parent_op = drop_ir->parents()[0];

  DCHECK(parent_op->IsRelationInit());
  Relation parent_relation = parent_op->relation();

  absl::flat_hash_set<std::string> dropped_columns;
  for (const auto& name : drop_ir->col_names()) {
    if (!parent_relation.HasColumn(name)) {
      return drop_ir->CreateIRNodeError("Column '$0' not found in parent dataframe", name);
    }
    dropped_columns.insert(name);
  }

  ColExpressionVector col_exprs;
  for (const auto& input_col_name : parent_relation.col_names()) {
    if (dropped_columns.contains(input_col_name)) {
      continue;
    }
    PL_ASSIGN_OR_RETURN(ColumnIR * column_ir,
                        ir_graph->CreateNode<ColumnIR>(drop_ir->ast_node(), input_col_name,
                                                       /*parent_op_idx*/ 0));
    column_ir->ResolveColumnType(parent_relation);
    col_exprs.emplace_back(input_col_name, column_ir);
  }

  // Init the map from the drop.
  PL_ASSIGN_OR_RETURN(MapIR * map_ir,
                      ir_graph->CreateNode<MapIR>(drop_ir->ast_node(), parent_op, col_exprs,
                                                  /* keep_input_columns */ false));
  PL_RETURN_IF_ERROR(map_ir->SetRelation(RelationFromExprs(map_ir->col_exprs())));

  // Update all of drop's dependencies to point to src.
  for (const auto& dep : drop_ir->Children()) {
    if (!dep->IsOperator()) {
      return drop_ir->CreateIRNodeError(
          "Received unexpected non-operator dependency on Drop node.");
    }
    auto casted_node = static_cast<OperatorIR*>(dep);
    PL_RETURN_IF_ERROR(casted_node->ReplaceParent(drop_ir, map_ir));
  }
  PL_RETURN_IF_ERROR(drop_ir->RemoveParent(parent_op));
  PL_RETURN_IF_ERROR(ir_graph->DeleteNode(drop_ir->id()));
  return true;
}

StatusOr<bool> SetupJoinTypeRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, RightJoin())) {
    PL_RETURN_IF_ERROR(ConvertRightJoinToLeftJoin(static_cast<JoinIR*>(ir_node)));
    return true;
  }
  return false;
}

void SetupJoinTypeRule::FlipColumns(const std::vector<ColumnIR*>& columns) {
  // Update the columns in the output_columns
  for (ColumnIR* col : columns) {
    DCHECK_LT(col->container_op_parent_idx(), 2);
    // 1 -> 0, 0 -> 1
    col->SetContainingOperatorParentIdx(1 - col->container_op_parent_idx());
  }
}

Status SetupJoinTypeRule::ConvertRightJoinToLeftJoin(JoinIR* join_ir) {
  DCHECK_EQ(join_ir->parents().size(), 2UL) << "There should be exactly two parents.";
  DCHECK(join_ir->join_type() == JoinIR::JoinType::kRight);

  std::vector<OperatorIR*> old_parents = join_ir->parents();
  for (OperatorIR* parent : old_parents) {
    PL_RETURN_IF_ERROR(join_ir->RemoveParent(parent));
  }

  PL_RETURN_IF_ERROR(join_ir->AddParent(old_parents[1]));
  PL_RETURN_IF_ERROR(join_ir->AddParent(old_parents[0]));

  FlipColumns(join_ir->left_on_columns());
  FlipColumns(join_ir->right_on_columns());
  FlipColumns(join_ir->output_columns());

  // TODO(philkuz) dependent upon how we actually do anything with output columns, this might change
  if (join_ir->suffix_strs().size() != 0) {
    DCHECK_EQ(join_ir->suffix_strs().size(), 2UL);
    std::string left = join_ir->suffix_strs()[0];
    std::string right = join_ir->suffix_strs()[1];
    join_ir->SetSuffixStrs({right, left});
  }

  return join_ir->SetJoinType(JoinIR::JoinType::kLeft);
}

StatusOr<bool> MergeGroupByIntoGroupAcceptorRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, OperatorWithParent(Operator(), GroupBy())) &&
      ir_node->type() == group_acceptor_type_) {
    return AddGroupByDataIntoGroupAcceptor(static_cast<GroupAcceptorIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> MergeGroupByIntoGroupAcceptorRule::AddGroupByDataIntoGroupAcceptor(
    GroupAcceptorIR* acceptor_node) {
  DCHECK_EQ(acceptor_node->parents().size(), 1UL);
  OperatorIR* parent = acceptor_node->parents()[0];
  DCHECK(Match(parent, GroupBy()));
  GroupByIR* groupby = static_cast<GroupByIR*>(parent);
  std::vector<ColumnIR*> new_groups(acceptor_node->groups());
  for (ColumnIR* g : groupby->groups()) {
    PL_ASSIGN_OR_RETURN(ColumnIR * col, CopyColumn(g));
    new_groups.push_back(col);
  }
  PL_RETURN_IF_ERROR(acceptor_node->SetGroups(new_groups));

  DCHECK_EQ(groupby->parents().size(), 1UL);
  OperatorIR* groupby_parent = groupby->parents()[0];

  PL_RETURN_IF_ERROR(acceptor_node->ReplaceParent(groupby, groupby_parent));

  return true;
}

StatusOr<ColumnIR*> MergeGroupByIntoGroupAcceptorRule::CopyColumn(ColumnIR* g) {
  if (Match(g, Metadata())) {
    return g->graph_ptr()->CreateNode<MetadataIR>(g->ast_node(), g->col_name(),
                                                  g->container_op_parent_idx());
  }

  return g->graph_ptr()->CreateNode<ColumnIR>(g->ast_node(), g->col_name(),
                                              g->container_op_parent_idx());
}

StatusOr<bool> RemoveGroupByRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, GroupBy())) {
    return RemoveGroupBy(static_cast<GroupByIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> RemoveGroupByRule::RemoveGroupBy(GroupByIR* groupby) {
  if (groupby->Children().size() != 0) {
    return groupby->CreateIRNodeError(
        "'groupby()' should be followed by an 'agg()' or a 'rolling()' not a $0",
        groupby->Children()[0]->type_string());
  }
  auto graph = groupby->graph_ptr();
  auto groupby_id = groupby->id();
  auto groupby_children = graph->dag().DependenciesOf(groupby->id());
  PL_RETURN_IF_ERROR(graph->DeleteNode(groupby_id));
  for (const auto& child_id : groupby_children) {
    PL_RETURN_IF_ERROR(graph->DeleteOrphansInSubtree(child_id));
  }
  return true;
}

StatusOr<bool> UniqueSinkNameRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, MemorySink())) {
    return false;
  }
  auto sink = static_cast<MemorySinkIR*>(ir_node);
  bool changed_name = false;
  if (sink_names_count_.contains(sink->name())) {
    sink->set_name(absl::Substitute("$0_$1", sink->name(), sink_names_count_[sink->name()]++));
    changed_name = true;
  } else {
    sink_names_count_[sink->name()] = 1;
  }
  return changed_name;
}

bool ContainsChildColumn(const ExpressionIR& expr,
                         const absl::flat_hash_set<std::string>& colnames) {
  if (Match(&expr, ColumnNode())) {
    return colnames.contains(static_cast<const ColumnIR*>(&expr)->col_name());
  }
  if (Match(&expr, Func())) {
    auto func = static_cast<const FuncIR*>(&expr);
    for (const auto& arg : func->args()) {
      if (ContainsChildColumn(*arg, colnames)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

bool CombineConsecutiveMapsRule::ShouldCombineMaps(
    MapIR* parent, MapIR* child, const absl::flat_hash_set<std::string>& parent_col_names) {
  // This rule is targeted at combining maps of the following style:
  // df.foo = 1
  // df.bar = df.abc * 2
  // The logic for combining maps where the child keeps input columns is different
  // from the logic where the child does not keep input columns, so for simplicity we only
  // solve the former case right now.
  if (!child->keep_input_columns()) {
    return false;
  }
  // If the parent has more than one child (one besides the child map), then we don't want
  // to combine them together because it could affect the output of the other child.
  if (parent->Children().size() > 1) {
    return false;
  }
  for (const auto& child_expr : child->col_exprs()) {
    if (ContainsChildColumn(*(child_expr.node), parent_col_names)) {
      return false;
    }
  }
  return true;
}

Status CombineConsecutiveMapsRule::CombineMaps(
    MapIR* parent, MapIR* child, const absl::flat_hash_set<std::string>& parent_col_names) {
  // If the column name is simply being overwritten, that's ok.
  for (const auto& child_col_expr : child->col_exprs()) {
    ExpressionIR* child_expr = child_col_expr.node;

    if (parent_col_names.contains(child_col_expr.name)) {
      // Overwrite it in the parent with the child if it's a name reassignment.
      for (const ColumnExpression& parent_col_expr : parent->col_exprs()) {
        if (parent_col_expr.name == child_col_expr.name) {
          PL_RETURN_IF_ERROR(parent->UpdateColExpr(parent_col_expr.name, child_expr));
        }
      }
    } else {
      // Otherwise just append it to the list.
      PL_RETURN_IF_ERROR(parent->AddColExpr(child_col_expr));
    }
    PL_RETURN_IF_ERROR(child_expr->graph_ptr()->DeleteEdge(child, child_expr));
  }

  PL_RETURN_IF_ERROR(child->RemoveParent(parent));
  for (auto grandchild : child->Children()) {
    PL_RETURN_IF_ERROR(grandchild->ReplaceParent(child, parent));
  }
  PL_RETURN_IF_ERROR(child->graph_ptr()->DeleteNode(child->id()));
  return Status::OK();
}

StatusOr<bool> CombineConsecutiveMapsRule::Apply(IRNode* ir_node) {
  // Roll the child into the parent so we only have to iterate over the graph once.
  if (!Match(ir_node, Map())) {
    return false;
  }
  auto child = static_cast<MapIR*>(ir_node);
  CHECK_EQ(child->parents().size(), 1UL);
  auto parent_op = child->parents()[0];

  if (!Match(parent_op, Map())) {
    return false;
  }
  auto parent = static_cast<MapIR*>(parent_op);

  absl::flat_hash_set<std::string> parent_cols;
  for (const auto& parent_expr : parent->col_exprs()) {
    parent_cols.insert(parent_expr.name);
  }

  if (!ShouldCombineMaps(parent, child, parent_cols)) {
    return false;
  }
  PL_RETURN_IF_ERROR(CombineMaps(parent, child, parent_cols));
  return true;
}

StatusOr<bool> NestedBlockingAggFnCheckRule::Apply(IRNode* ir_node) {
  // Roll the child into the parent so we only have to iterate over the graph once.
  if (!Match(ir_node, BlockingAgg())) {
    return false;
  }
  for (const auto& expr : static_cast<BlockingAggIR*>(ir_node)->aggregate_expressions()) {
    PL_RETURN_IF_ERROR(CheckExpression(expr));
  }
  return false;
}

Status NestedBlockingAggFnCheckRule::CheckExpression(const ColumnExpression& expr) {
  if (!Match(expr.node, Func())) {
    return expr.node->CreateIRNodeError("agg expression must be a function");
  }

  FuncIR* func = static_cast<FuncIR*>(expr.node);
  for (const auto& arg : func->args()) {
    if (arg->IsFunction()) {
      return arg->CreateIRNodeError("agg function arg cannot be a function");
    }
  }

  return Status::OK();
}

StatusOr<bool> PruneUnusedColumnsRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Operator())) {
    return false;
  }
  auto op = static_cast<OperatorIR*>(ir_node);
  DCHECK(op->IsRelationInit());
  auto changed = false;

  if (operator_to_required_outputs_.contains(op)) {
    auto required_outs = operator_to_required_outputs_.at(op);
    auto prev_relation = op->relation();
    PL_RETURN_IF_ERROR(op->PruneOutputColumnsTo(required_outs));
    auto new_relation = op->relation();
    changed = prev_relation != new_relation;
  }

  PL_ASSIGN_OR_RETURN(auto required_inputs, op->RequiredInputColumns());
  for (const auto& [parent_idx, required_columns] : Enumerate(required_inputs)) {
    auto parent_ptr = op->parents()[parent_idx];
    operator_to_required_outputs_[parent_ptr].insert(required_columns.begin(),
                                                     required_columns.end());
  }

  return changed;
}

StatusOr<bool> CleanUpStrayIRNodesRule::Apply(IRNode* ir_node) {
  auto ir_graph = ir_node->graph_ptr();
  auto node_id = ir_node->id();

  if (Match(ir_node, Operator()) || connected_nodes_.contains(ir_node)) {
    for (int64_t child_id : ir_graph->dag().DependenciesOf(node_id)) {
      connected_nodes_.insert(ir_graph->Get(child_id));
    }
    return false;
  }
  PL_RETURN_IF_ERROR(ir_graph->DeleteNode(node_id));
  return true;
}

StatusOr<bool> PruneUnconnectedOperatorsRule::Apply(IRNode* ir_node) {
  auto ir_graph = ir_node->graph_ptr();
  auto node_id = ir_node->id();

  if (Match(ir_node, MemorySink()) || sink_connected_nodes_.contains(ir_node)) {
    for (int64_t parent_id : ir_graph->dag().ParentsOf(node_id)) {
      sink_connected_nodes_.insert(ir_graph->Get(parent_id));
    }
    return false;
  }
  if (!Match(ir_node, Operator())) return false;
  std::vector<int64_t> nodes_to_remove;
  nodes_to_remove.push_back(node_id);

  // Remove child IR nodes that will become orphaned once the node is deleted.
  for (int64_t child_id : ir_graph->dag().DependenciesOf(node_id)) {
    auto child = ir_graph->Get(child_id);
    if (Match(child, Operator())) {
      continue;
    }
    // Remove a child if none of its children are Operators.
    auto child_child_ids = ir_graph->dag().DependenciesOf(child_id);
    if (!std::any_of(child_child_ids.begin(), child_child_ids.end(), [ir_graph](int64_t node_id) {
          return Match(ir_graph->Get(node_id), Operator());
        })) {
      nodes_to_remove.push_back(child_id);
    }
  }

  for (auto node_id : nodes_to_remove) {
    PL_RETURN_IF_ERROR(ir_graph->DeleteNode(node_id));
  }

  return true;
}

StatusOr<bool> DropMetadataColumnsFromSinksRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, MemorySink())) {
    return false;
  }
  auto sink = static_cast<MemorySinkIR*>(ir_node);
  auto col_names = sink->relation().col_names();
  std::vector<std::string> new_out_columns;
  Relation new_relation;
  for (const auto& [idx, col_name] : Enumerate(col_names)) {
    if (!absl::StartsWith(col_name, MetadataProperty::kMetadataColumnPrefix)) {
      new_relation.AddColumn(sink->relation().col_types()[idx], col_name);
      if (std::find(sink->out_columns().begin(), sink->out_columns().end(), col_name) !=
          sink->out_columns().end()) {
        new_out_columns.push_back(col_name);
      }
    }
  }
  if (new_relation == sink->relation()) {
    return false;
  }
  PL_RETURN_IF_ERROR(sink->SetOutColumns(new_out_columns));
  PL_RETURN_IF_ERROR(sink->SetRelation(new_relation));
  return true;
}

StatusOr<bool> AddLimitToMemorySinkRule::Apply(IRNode* ir_node) {
  if (!compiler_state_->has_max_output_rows_per_table()) {
    return false;
  }
  if (!Match(ir_node, MemorySink())) {
    return false;
  }
  auto mem_sink = static_cast<MemorySinkIR*>(ir_node);
  DCHECK_EQ(mem_sink->parents().size(), 1UL) << "There should be exactly one parent.";
  auto parent = mem_sink->parents()[0];

  // Update the current limit if it's too small
  if (Match(parent, Limit())) {
    auto limit = static_cast<LimitIR*>(parent);
    DCHECK(limit->limit_value_set());
    if (limit->limit_value() > compiler_state_->max_output_rows_per_table()) {
      limit->SetLimitValue(compiler_state_->max_output_rows_per_table());
      return true;
    }
    return false;
  }

  PL_ASSIGN_OR_RETURN(
      auto limit, mem_sink->graph_ptr()->CreateNode<LimitIR>(
                      mem_sink->ast_node(), parent, compiler_state_->max_output_rows_per_table()));
  PL_RETURN_IF_ERROR(mem_sink->ReplaceParent(parent, limit));
  return true;
}

OperatorIR* FilterPushdownRule::HandleMapPushdown(MapIR* map,
                                                  ColumnNameMapping* column_name_mapping) {
  ColumnNameMapping reverse_column_name_mapping;
  for (const auto& [old_name, cur_name] : *column_name_mapping) {
    reverse_column_name_mapping[cur_name] = old_name;
  }

  for (const auto& col_expr : map->col_exprs()) {
    // Ignore columns that aren't relevant to the particular filter func.
    if (!reverse_column_name_mapping.contains(col_expr.name)) {
      continue;
    }
    // If an involved column is actually created here, not the result of a reassignment
    // or rename, then we must stop walking up the tree and place the filter below
    // this parent Map.
    if (!Match(col_expr.node, ColumnNode())) {
      return nullptr;
    }

    // Check to see if the column has been renamed.
    // Update our column name map if it has.
    ColumnIR* column = static_cast<ColumnIR*>(col_expr.node);
    if (col_expr.name != column->col_name()) {
      auto eventual_col_name = reverse_column_name_mapping.at(col_expr.name);
      (*column_name_mapping)[eventual_col_name] = column->col_name();
    }
  }
  // If we didn't run into any of the above conditions, then we are save to move the
  // filter up above the current map.
  return map;
}

OperatorIR* FilterPushdownRule::HandleAggPushdown(BlockingAggIR* agg,
                                                  ColumnNameMapping* column_name_mapping) {
  ColumnNameMapping reverse_column_name_mapping;
  for (const auto& [old_name, cur_name] : *column_name_mapping) {
    reverse_column_name_mapping[cur_name] = old_name;
  }

  for (const auto& agg_expr : agg->aggregate_expressions()) {
    // If any of the filter columns come from the output of an aggregate expression,
    // don't push the filter up any further.
    // TODO(nserrino): For certain aggregate functions like min or max, it is actually
    // possible to push the aggregate up further. In order to support this, we need a
    // way of identifying whether or not a given aggregate func outputs the same type
    // of values that it receives (max(foo) returns an instance of a "foo" data point,
    // whereas count(foo) does not) and only push the filter up with that type.
    if (reverse_column_name_mapping.contains(agg_expr.name)) {
      return nullptr;
    }
  }

  // If all of the filter columns come from the group by column in an agg, then we are
  // safe to push the filter above the agg.
  return agg;
}

OperatorIR* FilterPushdownRule::NextFilterLocation(OperatorIR* current_node,
                                                   ColumnNameMapping* column_name_mapping) {
  // TODO(nserrino): Support pushing filters that originate from multiple parents.
  if (current_node->parents().size() != 1) {
    return nullptr;
  }

  OperatorIR* parent = current_node->parents()[0];
  if (Match(parent, Filter()) || Match(parent, Limit())) {
    return parent;
  }
  if (Match(parent, BlockingAgg())) {
    return HandleAggPushdown(static_cast<BlockingAggIR*>(parent), column_name_mapping);
  }
  if (Match(parent, Map())) {
    return HandleMapPushdown(static_cast<MapIR*>(parent), column_name_mapping);
  }
  // TODO(nserrino): Support joins and unions.
  return nullptr;
}

Status FilterPushdownRule::UpdateFilter(FilterIR* filter,
                                        const ColumnNameMapping& column_name_mapping) {
  PL_ASSIGN_OR_RETURN(ExpressionIR * new_expr,
                      filter->graph_ptr()->CopyNode(filter->filter_expr()));
  PL_ASSIGN_OR_RETURN(auto filter_input_cols, new_expr->InputColumns());
  for (ColumnIR* col : filter_input_cols) {
    auto new_col_name = column_name_mapping.at(col->col_name());
    col->UpdateColumnName(new_col_name);
  }
  return filter->SetFilterExpr(new_expr);
}

StatusOr<bool> FilterPushdownRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Filter())) {
    return false;
  }

  FilterIR* filter = static_cast<FilterIR*>(ir_node);
  OperatorIR* current_node = filter;

  // Tracks the name of each involved column we have in this filter func,
  // as of the current position of current_node.
  absl::flat_hash_map<std::string, std::string> column_name_mapping;
  PL_ASSIGN_OR_RETURN(auto involved_cols, filter->filter_expr()->InputColumnNames());
  if (!involved_cols.size()) {
    return false;
  }
  for (const auto& col : involved_cols) {
    column_name_mapping[col] = col;
  }

  // Iterate up from the current node, stopping when we reach the earliest allowable
  // new location for the filter node.
  while (OperatorIR* next_parent = NextFilterLocation(current_node, &column_name_mapping)) {
    current_node = next_parent;
  }

  if (current_node == filter) {
    return false;
  }

  PL_RETURN_IF_ERROR(UpdateFilter(filter, column_name_mapping));

  // Make the filter's parent its children's new parent.
  DCHECK_EQ(1, filter->parents().size());
  OperatorIR* filter_parent = filter->parents()[0];

  for (OperatorIR* child : filter->Children()) {
    PL_RETURN_IF_ERROR(child->ReplaceParent(filter, filter_parent));
  }
  PL_RETURN_IF_ERROR(filter->RemoveParent(filter_parent));

  DCHECK_EQ(1, current_node->parents().size());
  auto new_filter_parent = current_node->parents()[0];
  PL_RETURN_IF_ERROR(filter->AddParent(new_filter_parent));
  PL_RETURN_IF_ERROR(current_node->ReplaceParent(new_filter_parent, filter));
  PL_RETURN_IF_ERROR(filter->SetRelation(new_filter_parent->relation()));
  return true;
}

StatusOr<bool> PropagateExpressionAnnotationsRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Operator())) {
    return false;
  }
  auto op = static_cast<OperatorIR*>(ir_node);
  operator_output_annotations_[op] = {};

  bool updated_annotation = false;
  absl::flat_hash_set<ColumnIR*> dependent_columns;
  for (int64_t dependency_id : ir_node->graph_ptr()->dag().DependenciesOf(ir_node->id())) {
    auto node = ir_node->graph_ptr()->Get(dependency_id);
    if (Match(node, Expression())) {
      PL_ASSIGN_OR_RETURN(auto expr_columns, static_cast<ExpressionIR*>(node)->InputColumns());
      dependent_columns.insert(expr_columns.begin(), expr_columns.end());
    }
  }

  // For all of the input columns that this operator uses, fetch the annotations
  // that their referenced Operator knows about for each of them and add it to these instances.
  for (ColumnIR* col : dependent_columns) {
    PL_ASSIGN_OR_RETURN(auto referenced_op, col->ReferencedOperator());
    const auto& parent_annotations = operator_output_annotations_.at(referenced_op);
    if (!parent_annotations.contains(col->col_name())) {
      continue;
    }
    auto new_annotation = ExpressionIR::Annotations::Union(parent_annotations.at(col->col_name()),
                                                           col->annotations());
    if (new_annotation != col->annotations()) {
      updated_annotation = true;
      col->set_annotations(new_annotation);
    }
  }

  // Now set the annotations for this operator for each of its output columns, to be used
  // by child operators as input to computing its columns' annotations.
  if (Match(op, Join())) {
    auto join = static_cast<JoinIR*>(op);
    for (const auto& [output_col_idx, out_col_name] : Enumerate(join->column_names())) {
      operator_output_annotations_[op][out_col_name] =
          join->output_columns()[output_col_idx]->annotations();
    }
  } else if (Match(op, Union())) {
    // For each of the union output columns, compute the annotation that is comprised of
    // all of the fields that every parent shares.
    auto out_col_names = op->relation().col_names();
    for (const auto& out_col_name : out_col_names) {
      ExpressionIR::Annotations shared_annotation;
      // Note: assumes that unioning is done by column name with no renaming of columns.
      // supported. (same assumption as elsewhere)
      for (const auto& [parent_idx, parent] : Enumerate(op->parents())) {
        const auto& parent_annotations = operator_output_annotations_.at(parent);
        if (!parent_annotations.contains(out_col_name)) {
          continue;
        }
        auto input_annotation = parent_annotations.at(out_col_name);
        if (parent_idx == 0) {
          shared_annotation = input_annotation;
        } else {
          shared_annotation =
              ExpressionIR::Annotations::Intersection(shared_annotation, input_annotation);
        }
      }
      operator_output_annotations_[op][out_col_name] = shared_annotation;
    }
  } else if (Match(op, Map())) {
    for (const auto& col_expr : static_cast<MapIR*>(op)->col_exprs()) {
      operator_output_annotations_[op][col_expr.name] = col_expr.node->annotations();
    }
  } else if (Match(op, BlockingAgg())) {
    auto agg = static_cast<BlockingAggIR*>(op);
    for (const ColumnIR* group : agg->groups()) {
      operator_output_annotations_[op][group->col_name()] = group->annotations();
    }
    for (const ColumnExpression& expr : agg->aggregate_expressions()) {
      operator_output_annotations_[op][expr.name] = expr.node->annotations();
    }
  } else if (Match(op, Filter()) || Match(op, Limit())) {
    DCHECK_EQ(1, op->parents().size());
    operator_output_annotations_[op] = operator_output_annotations_.at(op->parents()[0]);
  }

  return updated_annotation;
}

StatusOr<bool> ResolveMetadataPropertyRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Metadata())) {
    return false;
  }
  auto metadata = static_cast<MetadataIR*>(ir_node);
  if (metadata->has_property()) {
    return false;
  }

  // Check to see whether metadata is valid.
  if (!md_handler_->HasProperty(metadata->name())) {
    return metadata->CreateIRNodeError("Specified metadata value '$0' is not properly handled.",
                                       metadata->name());
  }

  PL_ASSIGN_OR_RETURN(MetadataProperty * md_property, md_handler_->GetProperty(metadata->name()));
  metadata->set_property(md_property);
  return true;
}

Status ConvertMetadataRule::UpdateMetadataContainer(IRNode* container, MetadataIR* metadata,
                                                    ExpressionIR* metadata_expr) const {
  if (Match(container, Func())) {
    auto func = static_cast<FuncIR*>(container);
    for (const auto& [arg_idx, arg] : Enumerate(func->args())) {
      if (arg == metadata) {
        PL_RETURN_IF_ERROR(func->UpdateArg(arg_idx, metadata_expr));
      }
    }
    return Status::OK();
  }
  if (Match(container, Map())) {
    auto map = static_cast<MapIR*>(container);
    for (const auto& expr : map->col_exprs()) {
      if (expr.node == metadata) {
        PL_RETURN_IF_ERROR(map->UpdateColExpr(expr.name, metadata_expr));
      }
    }
    return Status::OK();
  }
  if (Match(container, Filter())) {
    auto filter = static_cast<FilterIR*>(container);
    return filter->SetFilterExpr(metadata_expr);
  }
  return error::Internal("Unsupported IRNode container for metadata: $0", container->DebugString());
}

StatusOr<std::string> ConvertMetadataRule::FindKeyColumn(const Relation& parent_relation,
                                                         MetadataProperty* property,
                                                         IRNode* node_for_error) const {
  DCHECK_NE(property, nullptr);
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

StatusOr<bool> ConvertMetadataRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Metadata())) {
    return false;
  }

  auto graph = ir_node->graph_ptr();
  auto metadata = static_cast<MetadataIR*>(ir_node);
  auto md_property = metadata->property();
  auto parent_op_idx = metadata->container_op_parent_idx();
  auto column_type = md_property->column_type();
  auto md_type = md_property->metadata_type();

  PL_ASSIGN_OR_RETURN(auto parent, metadata->ReferencedOperator());

  PL_ASSIGN_OR_RETURN(std::string key_column_name,
                      FindKeyColumn(parent->relation(), md_property, ir_node));

  PL_ASSIGN_OR_RETURN(
      ColumnIR * key_column,
      graph->CreateNode<ColumnIR>(ir_node->ast_node(), key_column_name, parent_op_idx));

  PL_ASSIGN_OR_RETURN(std::string func_name, md_property->UDFName(key_column_name));
  PL_ASSIGN_OR_RETURN(FuncIR * conversion_func,
                      graph->CreateNode<FuncIR>(ir_node->ast_node(),
                                                FuncIR::Op{FuncIR::Opcode::non_op, "", func_name},
                                                std::vector<ExpressionIR*>{key_column}));
  for (int64_t parent_id : graph->dag().ParentsOf(metadata->id())) {
    // For each container node of the metadata expression, update it to point to the
    // new conversion func instead.
    PL_RETURN_IF_ERROR(UpdateMetadataContainer(graph->Get(parent_id), metadata, conversion_func));
  }

  // Manually evaluate the column type, because DataTypeRule will run before this rule.
  PL_ASSIGN_OR_RETURN(auto evaled_col, DataTypeRule::EvaluateColumn(key_column));
  DCHECK(evaled_col);

  PL_ASSIGN_OR_RETURN(auto evaled_func,
                      DataTypeRule::EvaluateFunc(compiler_state_, conversion_func));
  DCHECK(evaled_func);
  DCHECK_EQ(conversion_func->EvaluatedDataType(), column_type)
      << "Expected the parent_relation key column type and metadata property type to match.";
  conversion_func->set_annotations(ExpressionIR::Annotations(md_type));

  return true;
}

}  // namespace planner
}  // namespace carnot
}  // namespace pl
