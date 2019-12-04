#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"

#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/compiler_state/registry_info.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/metadata_handler.h"

namespace pl {
namespace carnot {
namespace compiler {
class Rule {
 public:
  Rule() = delete;
  virtual ~Rule() = default;

  explicit Rule(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  /**
   * @brief Executes the rule defined in apply.
   *
   * @param ir_graph : the graph to operate on.
   * @return true: if the rule changes the graph.
   * @return false: if the rule does nothing to the graph.
   * @return Status: error if something goes wrong during the rule application.
   */
  virtual StatusOr<bool> Execute(IR* ir_graph);

 protected:
  /**
   * @brief Applies the rule to a node.
   * Should include a check for type and should return true if it changes the node.
   * It can pass potential compiler errors through the Status.
   *
   * @param ir_node - the node to apply this rule to.
   * @return true: if the rule changes the node.
   * @return false: if the rule does nothing to the node.
   * @return Status: error if something goes wrong during the rule application.
   */

  virtual StatusOr<bool> Apply(IRNode* ir_node) = 0;
  void DeferNodeDeletion(int64_t);
  Status EmptyDeleteQueue(IR* ir_graph);

  CompilerState* compiler_state_;

  // The queue containing nodes to delete.
  std::queue<int64_t> node_delete_q;
};

class DataTypeRule : public Rule {
  /**
   * @brief DataTypeRule evaluates the datatypes of any expressions that
   * don't have predetermined types.
   *
   * Currently resolves non-compile-time functions and column data types.
   *
   */

 public:
  explicit DataTypeRule(CompilerState* compiler_state) : Rule(compiler_state) {}

  /**
   * @brief Evaluates the datatype of column from the type data in the relation. Errors out if the
   * column doesn't exist in the relation.
   *
   * @param column the column to evaluate
   * @param relation the relation that should be used to evaluate the datatype of the column.
   * @return Status - error if column not contained in relation
   */
  static Status EvaluateColumnFromRelation(ColumnIR* column,
                                           const table_store::schema::Relation& relation);

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> EvaluateFunc(FuncIR* func) const;
  StatusOr<bool> EvaluateColumn(ColumnIR* column) const;
};

class SourceRelationRule : public Rule {
 public:
  explicit SourceRelationRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> GetSourceRelation(OperatorIR* source_op) const;
  StatusOr<std::vector<ColumnIR*>> GetColumnsFromRelation(
      OperatorIR* node, std::vector<std::string> col_names,
      const table_store::schema::Relation& relation) const;
  std::vector<int64_t> GetColumnIndexMap(const std::vector<std::string>& col_names,
                                         const table_store::schema::Relation& relation) const;
  StatusOr<table_store::schema::Relation> GetSelectRelation(
      IRNode* node, const table_store::schema::Relation& relation,
      const std::vector<std::string>& columns) const;
};

class OperatorRelationRule : public Rule {
  /**
   * @brief OperatorRelationRule sets up relations for non-source operators.
   */
 public:
  explicit OperatorRelationRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> SetBlockingAgg(BlockingAggIR* agg_ir) const;
  StatusOr<bool> SetMap(MapIR* map_ir) const;
  StatusOr<bool> SetMetadataResolver(MetadataResolverIR* map_ir) const;
  StatusOr<bool> SetUnion(UnionIR* union_ir) const;
  StatusOr<bool> SetOldJoin(JoinIR* join_op) const;
  StatusOr<bool> SetOther(OperatorIR* op) const;

  /**
   * @brief Setup the output columns for a Join. This is done before we create the relation of a
   * Join in case it doesn't have output columns set yet, as is the case with the pandas merge
   * syntax.
   *
   * What this method produces
   * 1. The columns order is left df columns in order followed by the right df columns in order.
   * 2. Join columns are not considered any differently -> both join columns will appear in the
   * resulting dataframe according to the columns.
   * 3. Duplicated column names will be suffixed with the Join's suffix labels
   * - If the duplicated column names are not unique after the suffixing, then it errors out.
   * - This is actually slightly different behavior than in Pandas but I don't think we can
   * replicate the resulting ability to index two columns at once. It's probably better to error out
   * than allow it. Proof that this is a feature: https://github.com/pandas-dev/pandas/pull/10639
   * 4. Will update the Join so that output columns are set _and have data type resolved_.
   *
   *
   * @param join_op the join operator to create output columns for.
   * @return Status
   */
  Status SetJoinOutputColumns(JoinIR* join_op) const;

  /**
   * @brief Create a Output Column IR nodes vector from the relations. Simply just copy the columns
   * from the parents.
   *
   * @param join_node the join node to error on in case of issues.
   * @param left_relation the left dataframe's relation.
   * @param right_relation the right dataframe's relation.
   * @param left_idx the index of the left dataframe, in case it was flipped
   * @param right_idx the index of the left dataframe, in case it was flipped
   * @return StatusOr<std::vector<ColumnIR*>> the columns as as vector or an error
   */
  StatusOr<std::vector<ColumnIR*>> CreateOutputColumnIRNodes(
      JoinIR* join_node, const table_store::schema::Relation& left_relation,
      const table_store::schema::Relation& right_relation) const;
  StatusOr<ColumnIR*> CreateOutputColumn(JoinIR* join_node, const std::string& col_name,
                                         int64_t parent_idx,
                                         const table_store::schema::Relation& relation) const;
  void ReplaceDuplicateNames(std::vector<std::string>* column_names, const std::string& dup_name,
                             const std::string& left_column, const std::string& right_column) const;
};

class EvaluateCompileTimeExpr {
  /**
   * @brief Takes an ExpressionIR node and traverses it to evaluate certain expressions at compile
   * time.
   * TODO(nserrino, philkuz) Generalize this beyond a few special cases.
   */

 public:
  explicit EvaluateCompileTimeExpr(CompilerState* compiler_state)
      : compiler_state_(compiler_state) {}

  StatusOr<ExpressionIR*> Evaluate(ExpressionIR* ir_node);

 private:
  StatusOr<IntIR*> EvalArithmetic(std::vector<ExpressionIR*> args, FuncIR* ir_node);
  StatusOr<IntIR*> EvalTimeNow(std::vector<ExpressionIR*> args, FuncIR* ir_node);
  StatusOr<IntIR*> EvalUnitTime(std::vector<ExpressionIR*> evaled_args, FuncIR* ir_node);

  CompilerState* compiler_state_;
};

class OperatorCompileTimeExpressionRule : public Rule {
  /**
   * @brief OperatorCompileTimeExpressionRule handles basic constant folding.
   *
   * ie
   * df['foo'] = pl.not_compile_time(pl.now() - pl.minutes(2))
   * should be evaluated to the following.
   * df['foo'] = pl.not_compile_time(15191289803)
   *
   */
 public:
  explicit OperatorCompileTimeExpressionRule(CompilerState* compiler_state)
      : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> EvalMap(MapIR* expr);
  StatusOr<bool> EvalFilter(FilterIR* expr);
  StatusOr<bool> EvalMemorySource(MemorySourceIR* expr);
  StatusOr<ExpressionIR*> EvalCompileTimeSubExpressions(ExpressionIR* expr);

  // Used to support taking strings like "-2m" into a range
  StatusOr<ExpressionIR*> EvalStringTimes(ExpressionIR* ir_node);
  StatusOr<IntIR*> EvalExpression(IRNode* ir_node, bool convert_string_times);
};

class VerifyFilterExpressionRule : public Rule {
  /**
   * @brief Quickly check to see whether filter expression returns True.
   *
   */
 public:
  explicit VerifyFilterExpressionRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
};

class ResolveMetadataRule : public Rule {
  /**
   * @brief Resolve metadata makes sure that metadata is properly added as a column in the table.
   * The job of this rule is to create, or map to a MetadataResolver for any MetadataIR in the
   * graph.
   */

 public:
  explicit ResolveMetadataRule(CompilerState* compiler_state, MetadataHandler* md_handler)
      : Rule(compiler_state), md_handler_(md_handler) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> HandleMetadata(MetadataIR* md_node) const;
  /** @brief Inserts a metadata resolver between the container op and parent op. */
  StatusOr<MetadataResolverIR*> InsertMetadataResolver(OperatorIR* container_op,
                                                       OperatorIR* parent_op) const;

 private:
  MetadataHandler* md_handler_;
};

class MetadataFunctionFormatRule : public Rule {
 public:
  explicit MetadataFunctionFormatRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<MetadataLiteralIR*> WrapLiteral(DataIR* data, MetadataProperty* md_property) const;
};

class CheckMetadataColumnNamingRule : public Rule {
  /**
   * @brief Checks to make sure that the column naming scheme doesn't collide with metadata.
   *
   * Never modifies the graph, so should always return false.
   */
 public:
  explicit CheckMetadataColumnNamingRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> CheckMapColumns(MapIR* op) const;
  StatusOr<bool> CheckAggColumns(BlockingAggIR* op) const;
};

class MetadataResolverConversionRule : public Rule {
  /**
   * @brief Converts the metadata resolver into a Map after everything is done.
   */
 public:
  explicit MetadataResolverConversionRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> ReplaceMetadataResolver(MetadataResolverIR* md_resolver) const;

  StatusOr<MapIR*> MakeMap(MetadataResolverIR* md_resolver) const;
  Status SwapInMap(MetadataResolverIR* md_resolver, MapIR* map) const;
  StatusOr<std::string> FindKeyColumn(const table_store::schema::Relation& parent_relation,
                                      MetadataProperty* property, IRNode* node_for_error) const;
  Status CopyParentColumns(IR* graph, OperatorIR* parent_op, ColExpressionVector* col_exprs,
                           pypa::AstPtr ast_node) const;

  Status AddMetadataConversionFns(IR* graph, MetadataResolverIR* md_resolver, OperatorIR* parent_op,
                                  ColExpressionVector* col_exprs) const;

  /**
   * @brief Removes the metadata resolver and pushes all of it's dependencies to depend on the
   * Metadata Resolver operator's parent.
   *
   * @param md_resolver: the metadata to remove from the graph.
   * @return Status: Error if there are issues with removing the node.
   */
  Status RemoveMetadataResolver(MetadataResolverIR* md_resolver) const;
  Status RemoveMap(MapIR* map) const;
  /**
   * @brief Returns true if the Map only copies the parent operation's columns.
   *
   * @param map: map_node
   * @return true: if the map returns a copy of the parent.
   */
  bool DoesMapOnlyCopy(MapIR* map) const;
};

class DropToMapOperatorRule : public Rule {
  /**
   * @brief Takes a DropIR and converts it to the corresponding Map IR.
   */
 public:
  explicit DropToMapOperatorRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> DropToMap(DropIR* drop_ir);
};

class SetupJoinTypeRule : public Rule {
  /**
   * @brief Converts a right join into a left join.
   *
   */
 public:
  SetupJoinTypeRule() : Rule(nullptr) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  /**
   * @brief Swaps the parents and updates any parent references within Join's children nodes.
   */
  Status ConvertRightJoinToLeftJoin(JoinIR* join_ir);
  void FlipColumns(const std::vector<ColumnIR*>& cols);
};

/**
 * @brief This rule finds every agg that follows a groupby and then copies the attributes
 * it contains into the aggregate.
 *
 * This rule is not responsible for removing groupbys that satisfy this condition - instead
 * RemoveGroupByRule handles this. There are cases where a groupby might be used by multiple aggs so
 * we can't remove them from the graph.
 *
 */
class MergeGroupByIntoAggRule : public Rule {
 public:
  MergeGroupByIntoAggRule() : Rule(nullptr) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> AddGroupByDataIntoAgg(BlockingAggIR* ir_node);
  StatusOr<ColumnIR*> CopyColumn(ColumnIR* g);
};

/**
 * @brief This rule removes groupbys after the aggregate merging step.
 *
 * We can't remove the groupby in the aggregate merging step because it's possible
 * that mutliple aggregates branch off of a single groupby. We need to do that here.
 *
 * This rule succeeds if it finds that GroupBy has no more children. If it does, this
 * rule will throw an error.
 *
 */
class RemoveGroupByRule : public Rule {
 public:
  RemoveGroupByRule() : Rule(nullptr) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> RemoveGroupBy(GroupByIR* ir_node);
};

/**
 * @brief This rule ensures that all sinks have unique names by appending
 * '_1', '_2', and so on ... to duplicates.
 *
 */
class UniqueSinkNameRule : public Rule {
 public:
  UniqueSinkNameRule() : Rule(nullptr) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  absl::flat_hash_map<std::string, int64_t> sink_names_count_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
