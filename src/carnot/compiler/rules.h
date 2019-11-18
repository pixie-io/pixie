#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

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
  StatusOr<bool> SetJoin(JoinIR* join_op) const;
  StatusOr<bool> SetOther(OperatorIR* op) const;
};

class EvaluateCompileTimeExprRule : public Rule {
  /**
   * @brief Takes an ExpressionIR node and traverses it to evaluate certain expressions at compile
   * time.
   * TODO(nserrino, philkuz) Generalize this beyond a few special cases.
   */

 public:
  explicit EvaluateCompileTimeExprRule(CompilerState* compiler_state) : Rule(compiler_state) {}
  // This rule needs to expose a different method than most rules, because its callers need
  // to be able to update their pointer to the new object returned by this rule.
  // TODO(nserrino,philkuz) Refactor rules to be ID-based, not pointer-based, to improve
  // composability with rules and make this exception unnecessary.
  StatusOr<ExpressionIR*> Evaluate(ExpressionIR* ir_node) {
    PL_ASSIGN_OR_RETURN(auto res, EvaluateExpr(ir_node));
    PL_RETURN_IF_ERROR(EmptyDeleteQueue(ir_node->graph_ptr()));
    return res;
  }

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<ExpressionIR*> EvaluateExpr(ExpressionIR* ir_node);
  StatusOr<IntIR*> EvalArithmetic(std::vector<ExpressionIR*> args, FuncIR* ir_node);
  StatusOr<IntIR*> EvalTimeNow(std::vector<ExpressionIR*> args, FuncIR* ir_node);
  StatusOr<IntIR*> EvalUnitTime(std::vector<ExpressionIR*> evaled_args, FuncIR* ir_node);
};

class RangeArgExpressionRule : public Rule {
  /**
   * @brief CompilerExpressionRule handles the execution of compiler functions
   * for things like Range operator arguments.
   *
   * ie
   * df.Range(start = plc.now() - plc.minutes(2), end = plc.now())
   * should be evaluated to the integer values.
   * df.Range(start = 15191289803, end = 15191500000)
   *
   */
 public:
  explicit RangeArgExpressionRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

  // Used to support taking strings like "-2m" into a range
  StatusOr<ExpressionIR*> EvalStringTimes(ExpressionIR* ir_node);
  StatusOr<IntIR*> EvalExpression(IRNode* ir_node);
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

class MergeRangeOperatorRule : public Rule {
  /**
   * @brief Takes a Range Operator and merges it with a MemorySource, removing the Range Operator
   * from the plan tree.
   */
 public:
  explicit MergeRangeOperatorRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> MergeRange(RangeIR* range_ir);
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

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
