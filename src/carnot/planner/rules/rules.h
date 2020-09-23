#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/metadata/metadata_handler.h"

namespace pl {
namespace carnot {
namespace planner {

template <typename TPlan>
struct RuleTraits {};

template <>
struct RuleTraits<IR> {
  using node_type = IRNode;
};

template <typename TPlan>
class BaseRule {
 public:
  BaseRule() = delete;
  BaseRule(CompilerState* compiler_state, bool use_topo, bool reverse_topological_execution)
      : compiler_state_(compiler_state),
        use_topo_(use_topo),
        reverse_topological_execution_(reverse_topological_execution) {}

  virtual ~BaseRule() = default;

  virtual StatusOr<bool> Execute(TPlan* graph) {
    bool any_changed = false;
    if (!use_topo_) {
      PL_ASSIGN_OR_RETURN(any_changed, ExecuteUnsorted(graph));
    } else {
      PL_ASSIGN_OR_RETURN(any_changed, ExecuteTopologicalSorted(graph));
    }
    PL_RETURN_IF_ERROR(EmptyDeleteQueue(graph));
    return any_changed;
  }

 protected:
  StatusOr<bool> ExecuteTopologicalSorted(TPlan* graph) {
    bool any_changed = false;
    std::vector<int64_t> topo_graph = graph->dag().TopologicalSort();
    if (reverse_topological_execution_) {
      std::reverse(topo_graph.begin(), topo_graph.end());
    }
    for (int64_t node_i : topo_graph) {
      // The node may have been deleted by a prior call to Apply on a parent or child node.
      if (!graph->HasNode(node_i)) {
        continue;
      }
      PL_ASSIGN_OR_RETURN(bool node_is_changed, Apply(graph->Get(node_i)));
      any_changed = any_changed || node_is_changed;
    }
    return any_changed;
  }

  StatusOr<bool> ExecuteUnsorted(TPlan* graph) {
    bool any_changed = false;
    // We need to copy over nodes because the Apply() might add nodes which can affect traversal,
    // causing nodes to be skipped.
    auto nodes = graph->dag().nodes();
    for (int64_t node_i : nodes) {
      // The node may have been deleted by a prior call to Apply on a parent or child node.
      if (!graph->HasNode(node_i)) {
        continue;
      }
      PL_ASSIGN_OR_RETURN(bool node_is_changed, Apply(graph->Get(node_i)));
      any_changed = any_changed || node_is_changed;
    }
    return any_changed;
  }

  /**
   * @brief Applies the rule to a node.
   * Should include a check for type and should return true if it changes the node.
   * It can pass potential compiler errors through the Status.
   *
   * @param node - the node to apply this rule to.
   * @return true: if the rule changes the node.
   * @return false: if the rule does nothing to the node.
   * @return Status: error if something goes wrong during the rule application.
   */
  virtual StatusOr<bool> Apply(typename RuleTraits<TPlan>::node_type* node) = 0;
  void DeferNodeDeletion(int64_t node) { node_delete_q.push(node); }

  Status EmptyDeleteQueue(TPlan* graph) {
    while (!node_delete_q.empty()) {
      PL_RETURN_IF_ERROR(graph->DeleteNode(node_delete_q.front()));
      node_delete_q.pop();
    }
    return Status::OK();
  }

  // The queue containing nodes to delete.
  std::queue<int64_t> node_delete_q;
  CompilerState* compiler_state_;
  bool use_topo_;
  bool reverse_topological_execution_;
};

using Rule = BaseRule<IR>;

class DataTypeRule : public Rule {
  /**
   * @brief DataTypeRule evaluates the datatypes of any expressions that
   * don't have predetermined types.
   *
   * Currently resolves non-compile-time functions and column data types.
   */

 public:
  explicit DataTypeRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

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
  /**
   * @brief Evaluates the datatype of an input func.
   */
  static StatusOr<bool> EvaluateFunc(CompilerState* compiler_state, FuncIR* func);
  /**
   * @brief Evaluates the datatype of an input column.
   */
  static StatusOr<bool> EvaluateColumn(ColumnIR* column);

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  static StatusOr<bool> EvaluateMetadata(MetadataIR* md);
};

class SourceRelationRule : public Rule {
 public:
  explicit SourceRelationRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

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
  explicit OperatorRelationRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> SetBlockingAgg(BlockingAggIR* agg_ir) const;
  StatusOr<bool> SetMap(MapIR* map_ir) const;
  StatusOr<bool> SetUnion(UnionIR* union_ir) const;
  StatusOr<bool> SetOldJoin(JoinIR* join_op) const;
  StatusOr<bool> SetMemorySink(MemorySinkIR* map_ir) const;
  StatusOr<bool> SetGRPCSink(GRPCSinkIR* map_ir) const;
  StatusOr<bool> SetRolling(RollingIR* rolling_ir) const;
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
   * replicate the resulting ability to index two columns at once. It's probably better to error
   * out than allow it. Proof that this is a feature:
   * https://github.com/pandas-dev/pandas/pull/10639
   * 4. Will update the Join so that output columns are set _and have data type resolved_.
   *
   *
   * @param join_op the join operator to create output columns for.
   * @return Status
   */
  Status SetJoinOutputColumns(JoinIR* join_op) const;

  /**
   * @brief Create a Output Column IR nodes vector from the relations. Simply just copy the
   * columns from the parents.
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
  StatusOr<IntIR*> EvalArithmetic(FuncIR* ir_node);
  StatusOr<IntIR*> EvalTimeNow(FuncIR* ir_node);
  StatusOr<IntIR*> EvalUnitTime(FuncIR* ir_node);

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
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> EvalMap(MapIR* expr);
  StatusOr<bool> EvalFilter(FilterIR* expr);
  StatusOr<bool> EvalMemorySource(MemorySourceIR* expr);
  StatusOr<bool> EvalRolling(RollingIR* rolling);
  StatusOr<ExpressionIR*> EvalCompileTimeSubExpressions(ExpressionIR* expr);

  StatusOr<IntIR*> EvalExpression(IRNode* ir_node, bool convert_string_times);
};

class ConvertStringTimesRule : public Rule {
  /**
   * @brief ConverStringTimesRuleUsed to support taking strings like "-2m"
   * into a memory source or a rolling operator. Currently special-cased in the system in order to
   * provide an ergonomic way to specify times in a memory source without disrupting the more
   * general constant-folding code in OperatorCompileTimeExpressionRule.
   * TODO(nserrino/philkuz): figure out if users will want to pass strings in as expressions
   * to memory sources or rolling operators that should NOT be converted to a time, and remove
   * this rule if so.
   *
   */
 public:
  inline static constexpr char kAbsTimeFormat[] = "%E4Y-%m-%d %H:%M:%E*S %z";
  explicit ConvertStringTimesRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> HandleMemSrc(MemorySourceIR* mem_src);
  StatusOr<bool> HandleRolling(RollingIR* rolling);
  bool HasStringTime(const ExpressionIR* expr);
  StatusOr<ExpressionIR*> ConvertStringTimes(ExpressionIR* expr, bool relative_time);
  StatusOr<int64_t> ParseDurationFmt(const StringIR* node, bool relative_time);
  StatusOr<int64_t> ParseAbsFmt(const StringIR* node);
  StatusOr<ExpressionIR*> ParseStringToTime(const StringIR* node, bool relative_time);
};

class SetMemSourceNsTimesRule : public Rule {
  /**
   * @brief SetMemSourceNsTimesRule is a simple rule to take the time expressions on memory
   * sources (which should have already been evaluated to an Int by previous rules) and sets the
   * nanosecond field that is used downstream.
   *
   */
 public:
  SetMemSourceNsTimesRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
};

class VerifyFilterExpressionRule : public Rule {
  /**
   * @brief Quickly check to see whether filter expression returns True.
   *
   */
 public:
  explicit VerifyFilterExpressionRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
};

class DropToMapOperatorRule : public Rule {
  /**
   * @brief Takes a DropIR and converts it to the corresponding Map IR.
   */
 public:
  explicit DropToMapOperatorRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

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
  SetupJoinTypeRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

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
 * @brief This rule finds every GroupAcceptorIR of the passed in type (i.e. agg or rolling) that
 * follows a groupby and then copies the groups of the groupby into the group acceptor.
 *
 * This rule is not responsible for removing groupbys that satisfy this condition - instead
 * RemoveGroupByRule handles this. There are cases where a groupby might be used by multiple aggs
 * so we can't remove them from the graph.
 *
 */
class MergeGroupByIntoGroupAcceptorRule : public Rule {
 public:
  explicit MergeGroupByIntoGroupAcceptorRule(IRNodeType group_acceptor_type)
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        group_acceptor_type_(group_acceptor_type) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> AddGroupByDataIntoGroupAcceptor(GroupAcceptorIR* acceptor_node);
  StatusOr<ColumnIR*> CopyColumn(ColumnIR* g);

  IRNodeType group_acceptor_type_;
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
  RemoveGroupByRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

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
  UniqueSinkNameRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  absl::flat_hash_map<std::string, int64_t> sink_names_count_;
};

/**
 * @brief This rule consolidates consecutive map nodes into a single node in cases where it is
 * simple to do so.
 *
 */
class CombineConsecutiveMapsRule : public Rule {
 public:
  CombineConsecutiveMapsRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  bool ShouldCombineMaps(MapIR* parent, MapIR* child,
                         const absl::flat_hash_set<std::string>& parent_col_names);
  Status CombineMaps(MapIR* parent, MapIR* child,
                     const absl::flat_hash_set<std::string>& parent_col_names);
};

/**
 * @brief This rule makes sure that nested blocking aggs cause a compiler error.
 *
 */
class NestedBlockingAggFnCheckRule : public Rule {
 public:
  NestedBlockingAggFnCheckRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  Status CheckExpression(const ColumnExpression& col_expr);
};

class PruneUnusedColumnsRule : public Rule {
 public:
  PruneUnusedColumnsRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ true) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  // For each operator, store the column names that it needs to output.
  absl::flat_hash_map<OperatorIR*, absl::flat_hash_set<std::string>> operator_to_required_outputs_;
};

/**
 * @brief This rule removes all (non-Operator) IR nodes that are not connected to an Operator.
 *
 */
class CleanUpStrayIRNodesRule : public Rule {
 public:
  CleanUpStrayIRNodesRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  // For each IRNode that stays in the graph, we keep track of its children as well so we
  // keep them around.
  absl::flat_hash_set<IRNode*> connected_nodes_;
};

/**
 * @brief This rule removes all Operator nodes that are not connected to a Memory Sink.
 *
 */
class PruneUnconnectedOperatorsRule : public Rule {
 public:
  PruneUnconnectedOperatorsRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ true) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  // For each IRNode that stays in the graph, we keep track of its children as well so we
  // keep them around.
  absl::flat_hash_set<IRNode*> sink_connected_nodes_;
};

/**
 * @brief This rule the window_size expression in RollingIR nodes
 *
 */
class ResolveWindowSizeRollingRule : public Rule {
 public:
  ResolveWindowSizeRollingRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  Status ResolveTimeString(StringIR* str_node);
};

/**
 * @brief This rule automatically adds a limit to all result sinks that are executed in batch.
 * Currently, that is all of our queries, but we will need to skip this rule in persistent,
 * streaming queries, when they are introduced.
 *
 */
class AddLimitToBatchResultSinkRule : public Rule {
 public:
  explicit AddLimitToBatchResultSinkRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
};

/**
 * @brief Propagate the expression annotations to downstream nodes when a column
 * is reassigned or used in another operator. For example, for an integer with annotations that is
 * assigned to be the value of a new column, that new column should have receive the same
 * annotations as the integer that created it.
 *
 * TODO(nserrino): Possibly move this rule to the distributed_planner, in the event that the
 * unions and GRPCSources that are generated in the distributed planner need to be given
 * annotations as well. For now, all of the nodes downstream of the union/grpc sources should be
 * annotated in those distributed plans from this rules in the single node analyzer.
 *
 */
class PropagateExpressionAnnotationsRule : public Rule {
 public:
  PropagateExpressionAnnotationsRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* node) override;
  // Status HandleMapAncestor(MapIR* map, std::string_view column_name);
  // Status HandleBlockingAggAncestor(BlockingAggIR* map, std::string_view column_name);
  // Status HandleJoinAncestor(JoinIR* map, std::string_view column_name);
  // Status HandleUnionAncestor(UnionIR* map, std::string_view column_name);

 private:
  using OperatorOutputAnnotations =
      absl::flat_hash_map<OperatorIR*, absl::flat_hash_map<std::string, ExpressionIR::Annotations>>;
  OperatorOutputAnnotations operator_output_annotations_;
};

class ResolveMetadataPropertyRule : public Rule {
  /**
   * @brief Resolve MetadataIR to a specific metadata property that the compiler knows about.
   */

 public:
  explicit ResolveMetadataPropertyRule(CompilerState* compiler_state, MetadataHandler* md_handler)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        md_handler_(md_handler) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  MetadataHandler* md_handler_;
};

class ConvertMetadataRule : public Rule {
  /**
   * @brief Converts the MetadataIR into the expression that generates it.
   */
 public:
  explicit ConvertMetadataRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  /**
   * @brief Updates any parents of the metadata node to point to the new metadata expression.
   */
  Status UpdateMetadataContainer(IRNode* container, MetadataIR* metadata,
                                 ExpressionIR* metadata_expr) const;
  StatusOr<std::string> FindKeyColumn(const table_store::schema::Relation& parent_relation,
                                      MetadataProperty* property, IRNode* node_for_error) const;
};

class ResolveTypesRule : public Rule {
  /**
   * @brief Resolves the types of operators.
   * It requires that group bys have already been merged into Aggs, and that metadataIR's have
   * already been converted into funcs.
   */
 public:
  explicit ResolveTypesRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
};

class ResolveStreamRule : public Rule {
  /**
   * @brief Resolves StreamIRs by setting their ancestor MemorySource nodes to streaming mode.
   */
 public:
  ResolveStreamRule()
      : Rule(/*compiler_state*/ nullptr, /*use_topo*/ false,
             /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
};

}  // namespace planner
}  // namespace carnot
}  // namespace pl
