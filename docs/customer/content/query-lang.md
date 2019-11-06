---
title: "Query Language"
metaTitle: "Pixie QL reference | Pixie"
metaDescription: "Query language documentation, examples, and use cases."
---

## Pixie QL 101 

#### Loading Data
Dataframes are made available in a query using the `From` operator. 

```python
table = dataframe(table="http_events")
```

#### Operating on the Data

Operators, such as Agg, are then called on Dataframes either by calling on a variable assigned the value of a previous Operation, 
or chained on directly to a previous Operation call.

```python
# Call aggregate on assigned variable
table = dataframe(table="http_events")
aggop = table.agg(by=r.upid, fn=lambda r:{
  'resp_latency_mean': pl.mean(r.http_resp_latency_ns)
}) 
```

```python
# Call aggregate directly on the output of the `From` Operator.
aggop = dataframe(table="http_events").agg(by=lambda r: r.upid, fn=lambda r:{
  'resp_latency_mean': pl.mean(r.http_resp_latency)
}) 
```

#### Combining Operators

Every operator produces a Dataframe that can then be passed to a new Operator, allowing you to write queries that chain
operations together

```python 
table = dataframe(table="http_events")
mapop = table.map(fn=lambda r: {
  'upid': r.upid,
  'http_resp_latency_ms': r.http_resp_latency_ns/1.0E6,
})
aggop = mapop.agg(by=lambda r: r.upid, fn=lambda r:{
  'resp_latency_mean': pl.mean(r.http_resp_latency_ms)
}) 
```
 #### Viewing the Data in the UI
Finally, to view the computed data in your data window, you must append a `Result(name="table_name")` Operator at the end.

```python
# Data created will now be passed up to the UI.
table = dataframe(table="http_events")
mapop = table.map(fn=lambda r: {
  'upid': r.upid,
  'http_resp_latency_ms': r.http_resp_latency_ns/1.0E6,
})
aggop = mapop.agg(by=lambda r: r.upid, fn=lambda r:{
  'resp_latency_mean': pl.mean(r.http_resp_latency_ms)
}).result(name="table_name")
```
