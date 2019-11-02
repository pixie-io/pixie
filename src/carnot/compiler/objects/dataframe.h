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

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
