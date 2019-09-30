---
title: "Scalar Functions"
metaTitle: "Pixie QL reference | Pixie"
metaDescription: "Query language documentation, examples, and use cases."
---

# Scalar Functions

## Description

These are functions that take in a scalar value or set of scalar values and outputs a single scalar value.

### Operators that use Scalar Functions
1. `Filter`
2. `Map`

| Scalar Function                     | Return Type |
| ----------------------------------- | ----------- |
| `pl.asid()`                         | `INT64`     |
| `pl.contains(STRING,STRING)`        | `BOOLEAN`   |
| `pl.find(STRING,STRING)`            | `INT64`     |
| `pl.length(STRING)`                 | `INT64`     |
| `pl.pluck(STRING,STRING)`           | `STRING`    |
| `pl.pluck_float64(STRING,STRING)`   | `FLOAT64`   |
| `pl.pluck_int64(STRING,STRING)`     | `INT64`     |
| `pl.pod_name_to_start_time(STRING)` | `TIME64NS`  |
| `pl.pod_name_to_status(STRING)`     | `STRING`    |
| `pl.substring(STRING,INT64,INT64)`  | `STRING`    |
| `pl.tolower(STRING)`                | `STRING`    |
| `pl.toupper(STRING)`                | `STRING`    |
| `pl.trim(STRING)`                   | `STRING`    |