#pragma once
#include <memory>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/compiler/objects/funcobject.h"

namespace pl {
namespace carnot {
namespace compiler {

/**
 * @brief Dataframe represents the processed data object in PixieQL. The API for the dataframe
 * object represents a subset of the Pandas API as well as some PixieQL specific operators.
 */
class Dataframe : public QLObject {
 public:
  static constexpr TypeDescriptor DataframeType = {
      /* name */ "Dataframe",
      /* type */ QLObjectType::kDataframe,
  };
  static StatusOr<std::shared_ptr<Dataframe>> Create(OperatorIR* op);

  /**
   * @brief Get the operator that this dataframe represents.
   *
   * @return OperatorIR*
   */
  OperatorIR* op() const { return op_; }

  /**
   * @brief Shortcut to get the IR graph that contains the operator.
   *
   * @return IR*
   */
  IR* graph() const { return op_->graph_ptr(); }

  // Method names.
  inline static constexpr char kRangeOpId[] = "range";
  inline static constexpr char kMapOpId[] = "map";
  inline static constexpr char kDropOpId[] = "drop";
  inline static constexpr char kFilterOpId[] = "filter";
  inline static constexpr char kBlockingAggOpId[] = "agg";
  inline static constexpr char kLimitOpId[] = "head";
  inline static constexpr char kMergeOpId[] = "merge";
  inline static constexpr char kGroupByOpId[] = "groupby";
  // Attribute names.
  inline static constexpr char kMetadataAttrName[] = "attr";

 protected:
  explicit Dataframe(OperatorIR* op) : QLObject(DataframeType, op), op_(op) {}
  StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr& ast,
                                                       const std::string& name) const override;

  Status Init();

 private:
  OperatorIR* op_;
};

/**
 * @brief Implements the join operator logic.
 *
 */
class JoinHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args);

 private:
  /**
   * @brief Converts column references (as list of strings or a string) into a vector of Columns.
   *
   * @param node the column reference node.
   * @param arg_name the name of the argument we are parsing. Used for error formatting.
   * @param parent_index the parent that these columns reference.
   * @return the columns refernced in the node or an error if processing something unexpected.
   */
  static StatusOr<std::vector<ColumnIR*>> ProcessCols(IRNode* node, std::string arg_name,
                                                      int64_t parent_index);
};

/**
 * @brief Implements the agg operator logic
 *
 */
class AggHandler {
 public:
  /**
   * @brief Evaluates the aggregate function. This only adds an aggregate by all node. If this
   * follows a groupby, then the analyzer will push the groupby into this node.
   *
   * @param df the dataframe to operate on
   * @param ast the ast node that signifies where the query was written
   * @param args the arguments for agg()
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args);

 private:
  static StatusOr<FuncIR*> ParseNameTuple(IR* ir, TupleIR* tuple);
};

/**
 * @brief Implements the range operator logic
 *
 */
class RangeHandler {
 public:
  /**
   * @brief Evaluates the range function by adding Range as a child of the df. The analyzer will
   * remove the Range function afterwards.
   *
   * @param df the dataframe to operate on
   * @param ast the ast node that signifies where the query was written
   * @param args the arguments for range()
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args);
};

/**
 * @brief Implements the drop operator logic
 *
 */
class DropHandler {
 public:
  /**
   * @brief Evaluates the drop operator logic. Downstream it will be converted to a map.
   *
   * @param df the dataframe to operate on
   * @param ast the ast node that signifies where the query was written
   * @param args the arguments for range()
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args);
};

/**
 * @brief Implements the limit operator logic.
 *
 */
class LimitHandler {
 public:
  /**
   * @brief Evaluates the limit method.
   *
   * @param df the dataframe that's a parent to the limit method.
   * @param ast the ast node that signifies where the query was written
   * @param args the arguments for limit()
   * @return StatusOr<QLObjectPtr>
   */

  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args);
};

class SubscriptHandler {
 public:
  /**
   * @brief Evaluates the subscript operator (filter and keep)
   *
   * @param df the dataframe that's a parent to the filter function.
   * @param ast the ast node that signifies where the query was written.
   * @param args the arguments
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args);

 private:
  static StatusOr<QLObjectPtr> EvalFilter(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                          ExpressionIR* expr);
  static StatusOr<QLObjectPtr> EvalKeep(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                        ListIR* cols);
  static StatusOr<QLObjectPtr> EvalColumn(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                          StringIR* cols);
};

/**
 * @brief Handles the groupby() method and creates the groupby node.
 *
 */
class GroupByHandler {
 public:
  /**
   * @brief Evaluates the groupby operator.
   *
   * @param df the dataframe that's a parent to the groupby function.
   * @param ast the ast node that signifies where the query was written.
   * @param args the arguments for groupby()
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args);

 private:
  static StatusOr<std::vector<ColumnIR*>> ParseByFunction(IRNode* by);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
