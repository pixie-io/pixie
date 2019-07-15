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
  } else if (match(ir_node, UnresolvedFunc())) {
    // Matches any function that has some unresolved args.
    VLOG(1) << absl::Substitute("$1(id=$0) has unresolved args.", ir_node->id(),
                                ir_node->type_string());
  } else if (match(ir_node, UnresolvedColumn())) {
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
    std::vector<IRNode*> select_children) const {
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
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
