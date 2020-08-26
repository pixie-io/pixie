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

  inline static constexpr char kDataFrameConstuctorDocString[] = R"doc(
  Setups up a DataFrame object from a table.

  Sets up the loading procedure of the table into the rest of the execution engine.
  The returned value can be transformed, aggregated and filtered using the DataFrame
  methods.

  Note that we are not actually loading data until the entire query is compiled, meaning
  that running this by itself won't do anything until a full pipeline is constructed.

  DataFrame is able to load in any set of tables. See `px.GetSchemas()` for a list of tables and
  the columns that can be loaded.

  :topic: dataframe_ops

  Args:
    table (string): The table name to load.
    select (Union[str,List[str]]): The columns of the table to load. Leave empty if you
      want to select all.
    start_time (px.Time): The earliest timestamp of data to load. Can be a relative time
      ie "-5m" or an absolute time in the following format "2020-07-13 18:02:5.00 +0000".
    end_time (px.Time): The last timestamp of data to load. Can be a relative time
      ie "-5m" or an absolute time in the following format "2020-07-13 18:02:5.00 +0000".

  Returns:
    px.DataFrame: DataFrame with the specified columns removed.
  )doc";

  // Method names.
  inline static constexpr char kMapOpID[] = "map";
  inline static constexpr char kDropOpID[] = "drop";
  inline static constexpr char kDropOpDocstring[] = R"doc(
  Drops the specified columns from the DataFrame.

  Returns a DataFrame with the specified columns dropped. Useful for removing
  columns you don't want to see in the final table result.  See [keep](#keep) on how
  to specify which columns to keep.

  :topic: dataframe_ops

  Args:
    columns (Union[str,List[str]]): DataFrame columns to drop, either as a string
      or a list.

  Returns:
    px.DataFrame: DataFrame with the specified columns removed.
  )doc";
  inline static constexpr char kKeepOpDocstring[] = R"doc(
  Keeps only the specified columns.

  Returns a DataFrame with only the specified columns. Useful for pruning
  columns to a small set before data is displayed. See `drop()` on how to drop
  specific columns instead.

  :topic: dataframe_ops

  Args:
    columns (List[str]): DataFrame columns to keep.

  Returns:
    px.DataFrame: DataFrame with the specified columns removed.
  )doc";

  inline static constexpr char kFilterOpID[] = "filter";
  // TODO(philkuz) update with the UDF docs link.
  inline static constexpr char kFilterOpDocstring[] = R"doc(
  Returns a DataFrame with only those rows that match the condition.

  Filters for the rows in the DataFrame that match the boolean condition. Will error
  out if you don't pass in a boolean expression. The functions available are defined in
  [UDFs](/udfs).

  :topic: dataframe_ops

  Args:
    key (bool): DataFrame expression to evaluate to a bool on each row. Rows that caus
      the expression to evaluate to true will be kept, otherwise they'll be removed.

  Returns:
    px.DataFrame: DataFrame with only those rows that return True for the expression.
  )doc";
  inline static constexpr char kBlockingAggOpID[] = "agg";
  // TODO(philkuz) update with the UDA docs link.
  inline static constexpr char kBlockingAggOpDocstring[] = R"doc(
  Aggregates the data based on the expressions.

  Computes the aggregate expressions on the data. If the preceding operator
  is a groupby, then we evaluate the aggregate expression in each group. If not, we
  calculate the aggregate expression using all of the preceding data. If no arguments
  specified and the call follows a grouped DataFrame, then agg() returns the unique groups.
  If following a non-grouped DataFrame agg() with no args will yield a compilation error.

  Each aggregate expression is simply the aggregate function applied to a column, formatted
  as `<out_col_name>=("<column_name>", <function>)`. A list of functions are available in the
  [UDA docs](/udas.)

  Examples:
    # Group by UPID and calculate maximum user time for the each
    # UPID group.
    df = px.DataFrame('process_stats')
    df = df.groupby('upid').agg(cpu_utime=('cpu_utime_ns', px.max))


  :topic: dataframe_ops

  Args:
    **kwargs (Tuple[string, AggFn]): The column, aggregate function pair that make up the
      expression to apply, assigned to the output column name. `<out_col_name>=("<column_name>", <function>)`.
      If this value is empty, it will return the unique groups in the previous DataFrame.

  Returns:
    px.DataFrame: DataFrame with aggregated expressions evaluated containing the groups (if any)
    followed by the output column aggregate expression names.
  )doc";
  inline static constexpr char kLimitOpID[] = "head";
  inline static constexpr char kLimitOpDocstring[] = R"doc(
  Return the first n rows.

  Returns a DataFrame with the first n rows of data.

  :topic: dataframe_ops

  Args:
    n (int): The number of rows to return. If not set, default is 5.

  Returns:
    px.DataFrame: DataFrame with the first n rows.
  )doc";

  inline static constexpr char kMergeOpID[] = "merge";
  inline static constexpr char kMergeOpDocstring[] = R"doc(
  Merges the input DataFrame with this one using a database-style join.

  Joins this DataFrame with the passed in right DataFrame according to the specified Join type.
  The DataFrame that we apply this on is the left DataFrame. The one passed in as an argument
  is the right DataFrame. If the join keys do not have the same type, this will error out.

  Examples:
    # Group by UPID and calculate maximum user time for the each
    # UPID group.
    left_df = px.DataFrame('process_stats', start_time='-10s')
    left_df = left_df.groupby('upid').agg(cpu_utime=('cpu_utime_ns', px.max))
    right_df = px.DataFrame('http_events', start_time='-10s)
    right_df = right_df.groupby('upid').agg(count=('http_resp_body', px.count))
    df = left_df.merge(right_df, how='inner', left_on='upid', right_on='upid',suffixes=['', '_x'])

    # df relation = ['upid', 'cpu_utime', 'upid_x', 'count']


  :topic: dataframe_ops

  Args:
    right (px.DataFrame):
    how (['inner', 'outer', 'left', 'right'], default 'inner'): the Type of merge to perform.
      * left: use the keys from the left DataFrame.
    left_on (string): Column name from this DataFrame.
    right_on (string): Column name from the right DataFarme to join on. Must be the same type as the `left_on` column.
    suffixes (Tuple[string, string], default ['_x', '_y']): The suffixes to apply to duplicate columns.

  Returns:
    px.DataFrame: Merged DataFrame with the relation
    [left_join_col, ...remaining_left_columns, ...remaining_right_columns].
  )doc";
  inline static constexpr char kGroupByOpID[] = "groupby";
  inline static constexpr char kGroupByOpDocstring[] = R"doc(
  Groups the data in preparation for an aggregate.


  Groups the data by the unique values in the passed in columns. At the current time we do not support
  standalone groupings, you must always follow the groupby() call with a call to [agg()](#agg).

  Examples:
    # Group by UPID and calculate maximum user time for the each
    # UPID group.
    df = px.DataFrame('process_stats')
    df = df.groupby('upid').agg(cpu_utime=('cpu_utime_ns', px.max))


  :topic: dataframe_ops

  Args:
    columns (Union[str,List[str]]): DataFrame columns to group by, either as a string
      or a list.

  Returns:
    px.DataFrame: Grouped DataFrame. Must be followed by a call to `agg()`.
  )doc";
  inline static constexpr char kUnionOpID[] = "append";

  inline static constexpr char kUnionOpDocstring[] = R"doc(
  Unions the passed in dataframes with this DataFrame.

  Unions the rows of the passed in DataFrames with this DataFrame. The passed
  in DataFrames. must have the same relation or `append` will throw a compilation error.
  Use `merge` to combine DataFrames with different relations.

  If there is a time column in the relation, `append` sorts the Unioned data by time.
  If there is no time column, then append will simply return a DataFrame with each
  DataFrame stacked on the other.

  Each aggregate expression is simply the aggregate function applied to a column, formatted
  as `<out_col_name>=("<column_name>", <function>)`. A list of functions are available in the
  [UDA docs](/udas.)

  Examples:
    df1 = px.DataFrame('process_stats', start_time='-10m', end_time='-9m')
    df2 = px.DataFrame('process_stats', start_time='-1m'

    df = df1.append(df2)


  :topic: dataframe_ops

  Args:
    other (px.DataFrame): The DataFrame to union with this one, relation must be the same.

  Returns:
    px.DataFrame: This DataFrame unioned with the passed in argument.
  )doc";
  inline static constexpr char kRollingOpID[] = "rolling";
  inline static constexpr char kRollingOpDocstring[] = R"doc(
  Groups the data by rolling windows.

  Rolls up data into groups based on the rolling window that it belongs to. Used to define
  window aggregates, the streaming analog of batch aggregates.

  Examples:
    df = px.DataFrame('process_stats')
    df = df.rolling('2s').agg(...)


  :topic: dataframe_ops

  Args:
    window (px.Duration): the size of the rolling window.

  Returns:
    px.DataFrame: DataFrame grouped into rolling windows. Must apply either a groupby or an aggregate on the
    returned DataFrame.
  )doc";
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
