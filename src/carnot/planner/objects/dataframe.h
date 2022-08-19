/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <memory>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/funcobject.h"

namespace px {
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
  static StatusOr<std::shared_ptr<Dataframe>> Create(CompilerState* compiler_state, OperatorIR* op,
                                                     ASTVisitor* visitor);
  static StatusOr<std::shared_ptr<Dataframe>> Create(CompilerState* compiler_state, IR* graph,
                                                     ASTVisitor* visitor);
  static bool IsDataframe(const QLObjectPtr& object) {
    return object->type() == DataframeType.type();
  }

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
  Sets up a DataFrame object from the specified table.

  Sets up the loading procedure of the table into the rest of the execution engine.
  The returned value can be transformed, aggregated and filtered using the DataFrame
  methods.

  Note that we are not actually loading data until the entire query is compiled, meaning
  that running this by itself won't do anything until a full pipeline is constructed.

  DataFrame is able to load in any set of tables. See `px.GetSchemas()` for a list of tables and
  the columns that can be loaded.

  :topic: dataframe_ops
  :opname: DataFrame

  Examples:
    # Select all columns
    df = px.DataFrame('http_events', start_time='-5m')
  Examples:
    # Select subset of columns.
    df = px.DataFrame('http_events', select=['upid', 'req_body'], start_time='-5m')
  Examples:
    # Absolute time specification.
    df = px.DataFrame('http_events', start_time='2020-07-13 18:02:5.00 -0700')
  Examples:
    # Absolute time sepecification (nanoseconds). Note this format only works for PxL scripts;
    # The Live UI's `start_time` argument does not support this format.
    df = px.DataFrame('http_events', start_time=1646157769000000000)

  Args:
    table (string): The table name to load.
    select (List[str]]): The columns of the table to load. Leave empty if you
      want to select all.
    start_time (px.Time): The earliest timestamp of data to load. The format can be one of the following:
      (1) relative time with format "-5m" or "-3h", (2) absolute time with format "2020-07-13 18:02:5.00 +0000",
      (3) absolute time in nanoseconds, or (4) `None`. Defaults to `None`. If `start_time` is `None`, then
      it begins with the first record in the table.
    end_time (px.Time): The last timestamp of data to load. The format can be one of the following:
      (1) relative time with format "-5m" or "-3h", (2) absolute time with format "2020-07-13 18:02:5.00 +0000",
      (3) absolute time in nanoseconds, or (4) `None`. Defaults to `None`. If `end_time` is `None` and `df.stream()`
      was not called on this DataFrame, then this DataFrame will process data until the last record that was in the table
      at the beginning of query execution. If `end_time` is `None` and `df.stream()` was called on this DataFrame,
      then this DataFrame will process data indefinitely.

  Returns:
    px.DataFrame: DataFrame loaded from the table with the specified columns and time period.
  )doc";

  // Method names.
  inline static constexpr char kMapOpID[] = "map";
  inline static constexpr char kMapOpDocstring[] = R"doc(
  Sets up the runtime expression and assigns the result to the specified column.

  Adds a column with the specified name and the expression that evaluates
  to the column value. The evaluation of this expression happens inside of the
  Pixie engine (Carnot) thus cannot be directly accessed during compilation.

  The expression can be a scalar value, a column from the same dataframe, or a
  [UDF](/reference/pxl/udf) function call. The syntax can be either `df['colname'] = expr`
  or `df.colname = expr`, the second option is simply syntactic sugar. The first
  form is slightly more expressive as you can set column names with spaces.


  Examples:
    df = px.DataFrame('process_stats')
    # Map scalar value to a column.
    df['number'] = 12
  Examples:
    df = px.DataFrame('http_events')
    df.svc = df.ctx['svc']
    # Map column to another column name.
    df.resp_body = df.resp_body
  Examples:
    df = px.DataFrame('http_events')
    # Map expression to the column.
    df['latency_ms'] = df['resp_latency_ns'] / 1.0e9


  :topic: dataframe_ops
  :opname: Map

  Args:
    column_name (str): The name of the column to assign this value.
    expr (ScalarExpression): The expression to evaluate in Carnot.

  Returns:
    px.DataFrame: DataFrame with the new column added.
  )doc";
  inline static constexpr char kDropOpID[] = "drop";
  inline static constexpr char kDropOpDocstring[] = R"doc(
  Drops the specified columns from the DataFrame.

  Returns a DataFrame with the specified columns dropped. Useful for removing
  columns you don't want to see in the final table result.  See `keep()` on how
  to specify which columns to keep.

  :topic: dataframe_ops
  :opname: Drop

  Examples:
    df = px.DataFrame('process_stats', select=['upid', 'cpu_ktime_ns', 'cpu_utime_ns'])
    # Drop upid from df.
    df = df.drop('upid')
  Examples:
    df = px.DataFrame('process_stats', select=['upid', 'cpu_ktime_ns', 'cpu_utime_ns'])
    # Drop upid an cpu_ktime_ns from df.
    df = df.drop(['upid', 'cpu_ktime_ns'])

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
  :opname: Keep

  Examples:
    df = px.DataFrame('process_stats', select=['upid', 'cpu_ktime_ns', 'rss_bytes'])
    # Keep only the upid and rss_bytes columns
    df = df[['upid', 'rss_bytes']]

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
  [UDFs](/reference/pxl/udf).

  Examples:
    df = px.DataFrame('http_events')
    # Filter for only http requests that are greater than 100 milliseconds
    df = df[df['resp_latency_ns'] > 100 * 1000 * 1000]

  :topic: dataframe_ops
  :opname: Filter

  Args:
    key (ScalarExpression): DataFrame expression that evaluates to a bool. Filter keeps
      any row that causes the expression to evaluate to True.

  Returns:
    px.DataFrame: DataFrame with only those rows that return True for the expression.
  )doc";
  inline static constexpr char kBlockingAggOpID[] = "agg";

  inline static constexpr char kBlockingAggOpDocstring[] = R"doc(
  Aggregates the data based on the expressions.

  Computes the aggregate expressions on the data. If the preceding operator
  is a groupby, then we evaluate the aggregate expression in each group. If not, we
  calculate the aggregate expression using all of the preceding data. If no arguments
  specified and the call follows a grouped DataFrame, then agg() returns the unique groups.
  If following a non-grouped DataFrame agg() with no args will yield a compilation error.

  Each aggregate expression is simply the aggregate function applied to a column, formatted
  as `<out_col_name>=("<column_name>", <function>)`. A list of functions are available in the
  [UDA docs](/reference/pxl/udf)

  Examples:
    # Group by UPID and calculate maximum user time for the each
    # UPID group.
    df = px.DataFrame('process_stats')
    df = df.groupby('upid').agg(cpu_utime=('cpu_utime_ns', px.max))

  :topic: dataframe_ops
  :opname: Agg

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
  :opname: Limit

  Examples:
    df = px.DataFrame('http_events')
    # Keep only the first 100 http requests.
    df = df.head(100)

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
    # Single join key: Group by UPID and calculate maximum user time for the each UPID group.
    left_df = px.DataFrame('process_stats', start_time='-10s')
    left_df = left_df.groupby('upid').agg(cpu_utime=('cpu_utime_ns', px.max))
    right_df = px.DataFrame('http_events', start_time='-10s')
    right_df = right_df.groupby('upid').agg(count=('resp_body', px.count))
    df = left_df.merge(right_df, how='inner', left_on='upid', right_on='upid', suffixes=['', '_x'])
    # Output relation: ['upid', 'cpu_utime', 'upid_x', 'count']
    # Multiple join keys: Calculate maximum user time for the each service/node pair.
    left_df = px.DataFrame('process_stats', start_time='-10s')
    left_df.node = left_df.ctx['node']
    left_df.service = left_df.ctx['service']
    left_df = left_df.groupby(['service', 'node']).agg(cpu_utime=('cpu_utime_ns', px.max))
    right_df = px.DataFrame('http_events', start_time='-10s')
    right_df.node = right_df.ctx['node']
    right_df.service = right_df.ctx['service']
    right_df = right_df.groupby(['service', 'node']).agg(count=('resp_body', px.count))
    df = left_df.merge(right_df, how='inner', left_on=['service', 'node'], right_on=['service', 'node'], suffixes=['', '_x'])
    # Output relation: ['service', 'node', 'cpu_utime', 'service_x', 'node_x', 'count']

  :topic: dataframe_ops
  :opname: Join

  Args:
    right (px.DataFrame): The DataFrame to join with this DataFrame.
    how (['inner', 'outer', 'left', 'right'], default 'inner'): the type of merge (join) to perform.
      * inner: use the intersection of the left and right keys.
      * outer: use the union of the left and right keys.
      * left: use the keys from the left DataFrame.
      * right: use the keys from the right DataFrame.
    left_on (Union[string, List[string]]): Column name from this DataFrame, either as a string or a list of strings.
    right_on (Union[string, List[string]]): Column name from the right DataFarme to join on. Must be the same type as the `left_on` column.
    suffixes (Tuple[string, string], default ['_x', '_y']): The suffixes to apply to duplicate columns.

  Returns:
    px.DataFrame: Merged DataFrame with the relation
    [left_join_col, ...remaining_left_columns, ...remaining_right_columns].
  )doc";
  inline static constexpr char kGroupByOpID[] = "groupby";
  inline static constexpr char kGroupByOpDocstring[] = R"doc(
  Groups the data in preparation for an aggregate.


  Groups the data by the unique values in the passed in columns. At the current time we do not support
  standalone groupings, you must always follow the groupby() call with a call to `agg()`.

  Examples:
    # Group by UPID and calculate maximum user time for the each
    # UPID group.
    df = px.DataFrame('process_stats')
    df = df.groupby('upid').agg(cpu_utime=('cpu_utime_ns', px.max))

  :topic: dataframe_ops
  :opname: Group By

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

  Examples:
    df1 = px.DataFrame('process_stats', start_time='-10m', end_time='-9m')
    df2 = px.DataFrame('process_stats', start_time='-1m')
    df = df1.append(df2)

  :topic: dataframe_ops
  :opname: Union

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
  :opname: Rolling Window

  Args:
    window (px.Duration): the size of the rolling window.

  Returns:
    px.DataFrame: DataFrame grouped into rolling windows. Must apply either a groupby or an aggregate on the
    returned DataFrame.
  )doc";

  inline static constexpr char kStreamOpId[] = "stream";
  inline static constexpr char kStreamOpDocstring[] = R"doc(
  Execute this DataFrame in streaming mode.

  Returns the input DataFrame, but set to streaming mode. Streaming queries execute indefinitely,
  as opposed to batch queries which return a finite result.

  Examples:
    df = px.DataFrame('http_events').stream()

  :topic: dataframe_ops
  :opname: Stream

  Returns:
    px.DataFrame: the parent DataFrame in streaming mode.
  )doc";

  // Attribute names.
  inline static constexpr char kMetadataAttrName[] = "ctx";

  StatusOr<std::shared_ptr<Dataframe>> FromColumnAssignment(CompilerState* compiler_state,
                                                            const pypa::AstPtr& expr_node,
                                                            ColumnIR* column, ExpressionIR* expr);

 protected:
  explicit Dataframe(CompilerState* compiler_state, OperatorIR* op, IR* graph, ASTVisitor* visitor)
      : QLObject(DataframeType, op ? op->ast() : nullptr, visitor),
        compiler_state_(compiler_state),
        op_(op),
        graph_(graph) {}
  StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr& ast,
                                                       std::string_view name) const override;

  Status Init();
  bool HasNonMethodAttribute(std::string_view /* name */) const override { return true; }

 private:
  CompilerState* compiler_state_;
  OperatorIR* op_ = nullptr;
  IR* graph_ = nullptr;
};

StatusOr<std::shared_ptr<Dataframe>> GetAsDataFrame(QLObjectPtr obj);

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
