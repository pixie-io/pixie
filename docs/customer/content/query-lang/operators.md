---
title: "Operators"
metaTitle: "Pixie QL reference | Pixie"
metaDescription: "Pixie query language Operators reference."
---
## dataframe()
`dataframe(String table, List<String> select=[])`

#### Example
```python
df = dataframe(table="http_events", select=[
  "upid", 
  "http_resp_status", 
  "http_resp_latency_ns"
])
```

#### Description

`dataframe` accesses a table from memory and makes it available for Operators to actually process the data. This is the only operator 
that can be called independent of a Dataframe. The output relation is defined by the selected columns, or if the select argument is 
left out, then the relation of the original table.

#### Arguments

* `table`: (*type: str*): the table name to pull up. This must be the same as the tables available in the [Table Schema](/user-guides/data-schema.md)
* `select`:(*type: list of str*) ***optional***: list of columns to grab from the table. Each column name must exist in the table. 
If not provided, the From operator will assume all columns are selected.

#### result 

Returns a Dataframe with a relation containing the columns specified by the `select` argument, or if not specified, the full relation of the 
referenced table.

## range()

`df.range(Time64NS start, Time64NS stop=plc.now())`

#### Example

```python
df = dataframe(table="http_events")
# Get the data from 10 minutes ago up to 5 minutes ago.
df.range(start="-10m",stop="-5m")
```

#### Description

`range` enables users to query a specific range of time of data. The start and stop times can be queried in several different ways, which is discussed at length
in the [Time Specification](/user-guides/time-in-queries). This operator can currently only be placed immediately after the `From` operator, otherwise it will throw an error.

#### Arguments

* `start` (*type time64ns(int64)*): the start of the range of timestamps we wish to get from the source table. 
Can be specified with all of the methods detailed in [Time Specification](/user-guides/time-in-queries).

* `stop` (*type time64ns(int64)*) ***optional***: the end of the range of timestamps to get from the source table. 
Can be specified with all of the methods detailed in [Time Specification](/user-guides/time-in-queries). If left unspecified will be set to the current timestamp. 

#### Returns

A Dataframe with the same relation as the Parent operator. The represented data only contains those rows that fit in the specified range.

## result()

`df.result(String name)`

#### Example

```python
df = dataframe(table="http_events")
df.result(name="table_name")
```

#### Description

Queries that write to the data window must call `Result` to yield the Dataframe it is called upon in UI table view. 
For now, calling `Result` multiple times in a query is unsupported behavior and can cause unexpected functionality.
Also, the output name currently does not have functionality, but in the future we will use the name within the UI.

Note that at the moment, this operator does not make the table available for query by a separate `From` operator.

#### Arguments

* `name` (*type str*): the name of the table to write out to. At the current moment, this doesn't do anything and will be removed soon.

#### Returns

Nothing

## map()

`df.map(Lambda fn)`

#### Example

```python
df = dataframe(table="http_events")
df.map(fn=lambda r: {
  'upid': r.upid, # copy the column. 
  'http_resp_latency_ms': r.http_resp_latency_ns / 1.0E6, # apply an arithmetic operation.
  'http_body_first10': pl.substring(r.http_body, 0, 10), # apply a UDF to a column.
})
```

#### Description

`Map` performs a projection on the Dataframe, mapping the original column values to a set of new column values according to scalar functions. 

#### Arguments

* `fn` (*type lambda*): a lambda function that specifies the expressions that create the output Dataframe. The body 
of this lambda function must be a dictionary where the keys are the new column names and the values are the expressions. The set of functions that you can execute are 
listed in the [Scalar Functions document](/user-guides/functions).

#### Returns

A Dataframe that is the result of this Operator's projections, as described by the `fn` argument.
The size of the relation should be equal to the number of dictionary elements specified in `fn`.

## agg()

`df.agg(Lambda by, Lambda fn)`

#### Example

```python
df = dataframe(table="http_events")
df.agg(
  by=lambda r: [r.upid, r.http_resp_status],
  fn=lambda r: {
    "avg_resp_latency": pl.mean(r.http_resp_latency_ns),
  }
)
```

#### Description

`agg` groups by the specified columns and aggregates columns according to the aggregate expressions. 

#### Arguments

* `fn` (*type lambda*): a lambda function that specifies the aggregate expressions that create the output Dataframe. The body 
of this lambda function must be a dictionary where the keys are the new column names and the values are the aggregate expressions.
The available aggregate functions are listed in the [Aggregate Functions document](/user-guides/functions).

* `by` (*type lambda*) ***optional***: a lambda function that specifies the group by columns. The body can either be
a Column or a list of Columns. If `by` is not specified, then the Aggregate groups by all and returns only those columns specified in the `fn`.

#### Returns

A Dataframe that is the result of aggregation. The first columns in the resulting Dataframe's relation correspond to the columns 
of the `by` argument, while the remaining columns are the result of the aggregate expressions defined in the `fn` argument.

## filter()

`df.filter(Lambda fn)`

#### Example

```python
df = dataframe(table="http_events")
df.filter(fn=lambda r: r.http_resp_latency_ns / 1.0E6 >= 10)
```

#### Description

`Filter` processes a parent Dataframe and only keeps rows that match a specified condition.

#### Arguments

* `fn` (*type lambda*): a lambda function that contains the condition to Filter by. The body of the lambda must be a non-dictionary expression
that evaluates to a boolean value, otherwise will error out.

#### Returns

A Dataframe with the same relation as the parent, but only contains the rows that match the filter condition. 

## merge()

`left_table.merge(Dataframe right_table, String type, Lambda cond, Lambda cols)`

#### Example

```python
table1 = dataframe(table="http_events", select=["upid", "http_resp_latency_ns"])
table2 = dataframe(table="process_stats", select=["upid", "cpu_ktime_ns"])
table1.merge(table2, type='inner', 
                    cond=lambda r1, r2: r1.upid == r2.upid,
                    cols=lambda r1, r2: {
                      'upid': r1.upid, 
                      'http_resp_latency_ns': r1.http_resp_latency_ns, 
                      'cpu_ktime_ns': r2.cpu_ktime_ns
                    }).result(name="table_name")
```

#### Description

`merge` specifies an equijoin of two Tables, preserving the columns from each table specified in the `cols` argument. 

#### Arguments

* `right_table` (*type Dataframe*): the Table to `merge` with the table that the operator is called upon. The parent Dataframe on which the `merge` is called 
on is the left `Dataframe` while `right_table` represents the right table. 

* `type` (*type string*): defines the type of merge to be done. Allows ("inner", "outer", "left", "right")

* `cond` (*type lambda*): a two argument lambda that defines on what conditions a left and right row should be joined. This currently
supports only equality condition or equality conditions that are AND'd together.

* `cols` (*type lambda*): a two argument lambda that defines the output of the join operation. The structure of this dictionary defines the output relation. The values
must strictly be Column expressions, otherwise the query will cause the compiler to throw an error.

#### Returns

A Dataframe with the columns specified by the `cols` argument.

## limit()

`df.limit(int rows)`

#### Example

```python
df = dataframe(table="http_events")
df.limit(rows=100)
```

#### Description

`limit` only passes down the specified number of rows from the parent operator.

#### Arguments

* `rows` (*type int*): the maximum number of rows to allow from the parent Dataframe.

#### Returns

A Dataframe with the same relation as the parent, but only containing the specified number of rows.
