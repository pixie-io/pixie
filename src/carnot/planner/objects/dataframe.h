#pragma once
#include <memory>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/funcobject.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief Dataframe represents the processed data object in PixieQL. The API for the dataframe
 * object represents a subset of the Pandas API as well as some PixieQL specific operators.
 */
class Dataframe : public QLObject {
 public:
  static constexpr TypeDescriptor DataframeType = {
      /* name */ "DataFrame",
      /* type */ QLObjectType::kDataframe,
  };
  static StatusOr<std::shared_ptr<Dataframe>> Create(OperatorIR* op, ASTVisitor* visitor);
  static StatusOr<std::shared_ptr<Dataframe>> Create(IR* graph, ASTVisitor* visitor);

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
  IR* graph() const { return graph_; }

  // Method names.
  inline static constexpr char kMapOpId[] = "map";
  inline static constexpr char kDropOpId[] = "drop";
  inline static constexpr char kFilterOpId[] = "filter";
  inline static constexpr char kBlockingAggOpId[] = "agg";
  inline static constexpr char kLimitOpId[] = "head";
  inline static constexpr char kMergeOpId[] = "merge";
  inline static constexpr char kGroupByOpId[] = "groupby";
  inline static constexpr char kUnionOpId[] = "append";
  inline static constexpr char kRollingOpId[] = "rolling";
  // Attribute names.
  inline static constexpr char kMetadataAttrName[] = "ctx";

  StatusOr<std::shared_ptr<Dataframe>> FromColumnAssignment(const pypa::AstPtr& expr_node,
                                                            ColumnIR* column, ExpressionIR* expr);

 protected:
  explicit Dataframe(OperatorIR* op, IR* graph, ASTVisitor* visitor)
      : QLObject(DataframeType, op, visitor), op_(op), graph_(graph) {}
  StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr& ast,
                                                       std::string_view name) const override;

  Status Init();
  bool HasNonMethodAttribute(std::string_view /* name */) const override { return true; }

 private:
  OperatorIR* op_ = nullptr;
  IR* graph_ = nullptr;
};

/**
 * @brief Implements the join operator logic.
 *
 */
class JoinHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);

 private:
  /**
   * @brief Converts column references (as list of strings or a string) into a vector of Columns.
   *
   * @param graph the IR graph
   * @param ast the AST node
   * @param obj the column reference obj.
   * @param arg_name the name of the argument we are parsing. Used for error formatting.
   * @param parent_index the parent that these columns reference.
   * @return the columns refernced in the node or an error if processing something unexpected.
   */
  static StatusOr<std::vector<ColumnIR*>> ProcessCols(IR* graph, const pypa::AstPtr& ast,
                                                      QLObjectPtr obj, std::string arg_name,
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
                                    const ParsedArgs& args, ASTVisitor* visitor);

 private:
  static StatusOr<FuncIR*> ParseNameTuple(IR* ir, const pypa::AstPtr& ast,
                                          std::shared_ptr<TupleObject> tuple);
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
   * @param args the arguments for drop()
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
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
                                    const ParsedArgs& args, ASTVisitor* visitor);
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
                                    const ParsedArgs& args, ASTVisitor* visitor);

 private:
  static StatusOr<QLObjectPtr> EvalFilter(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                          ExpressionIR* expr, ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> EvalKeep(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                        std::shared_ptr<CollectionObject> cols,
                                        ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> EvalColumn(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                          StringIR* cols, ASTVisitor* visitor);
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
                                    const ParsedArgs& args, ASTVisitor* visitor);

 private:
  static StatusOr<std::vector<ColumnIR*>> ParseByFunction(IRNode* by);
};

/**
 * @brief Handles the append() method and creates the union node.
 *
 */
class UnionHandler {
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
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

/**
 * @brief Handles the rolling() method and creates the rolling node.
 *
 */
class RollingHandler {
 public:
  /**
   *  @brief Evaluates the rolling operator.
   * @param df the dataframe that's a parent to the rolling function.
   * @param ast the ast node that signifies where the query was written.
   * @param args the arguments for rolling()
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

/**
 * @brief Implements the DataFrame() constructor logic.
 *
 */
class DataFrameHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
