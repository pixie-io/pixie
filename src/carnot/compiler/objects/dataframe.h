#pragma once
#include <memory>
#include <string>
#include <vector>

#include <pypa/ast/ast.hh>
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

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

  explicit Dataframe(OperatorIR* op);

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

  inline static constexpr char kRangeOpId[] = "range";
  inline static constexpr char kMapOpId[] = "map";
  inline static constexpr char kFilterOpId[] = "filter";
  inline static constexpr char kBlockingAggOpId[] = "agg";
  inline static constexpr char kRangeAggOpId[] = "range_agg";
  inline static constexpr char kLimitOpId[] = "limit";
  inline static constexpr char kMergeOpId[] = "merge";

 private:
  OperatorIR* op_;
};

/**
 * @brief Implements the join operator logic.
 *
 */
class JoinHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(Dataframe* df, const pypa::AstPtr& ast, const ParsedArgs& args);

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
  static StatusOr<QLObjectPtr> Eval(Dataframe* df, const pypa::AstPtr& ast, const ParsedArgs& args);

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
  static StatusOr<QLObjectPtr> Eval(Dataframe* df, const pypa::AstPtr& ast, const ParsedArgs& args);
};

// TODO(philkuz) (PL-1036) remove this upon availability of new syntax.
/**
 * @brief Implements the old map operator logic. This will be deprecated soon - but to reduce the
 * amount of changes for a pyobject switch over this makes it easier.
 *
 */
class OldMapHandler {
 public:
  /**
   * @brief Evaluates the old map function.
   *
   * @param df the dataframe that's a parent to the map function.
   * @param ast the ast node that signifies where the query was written
   * @param args the arguments for map()
   * @return StatusOr<QLObjectPtr>
   */

  static StatusOr<QLObjectPtr> Eval(Dataframe* df, const pypa::AstPtr& ast, const ParsedArgs& args);
};

// TODO(philkuz) (PL-1039) remove this upon availability of new syntax.
/**
 * @brief Implements the old filter operator logic. This will be deprecated soon - but to reduce the
 * amount of changes for a pyobject switch over this makes it easier.
 *
 */
class OldFilterHandler {
 public:
  /**
   * @brief Evaluates the old filter function.
   *
   * @param df the dataframe that's a parent to the filter function.
   * @param ast the ast node that signifies where the query was written
   * @param args the arguments for filter()
   * @return StatusOr<QLObjectPtr>
   */

  static StatusOr<QLObjectPtr> Eval(Dataframe* df, const pypa::AstPtr& ast, const ParsedArgs& args);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
