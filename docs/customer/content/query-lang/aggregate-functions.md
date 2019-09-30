---
title: "Aggregate Functions"
metaTitle: "Pixie QL reference | Pixie"
metaDescription: "Query language documentation, examples, and use cases."
---

# Aggregate functions

## Description

These are functions that take in multiple values and output a single scalar value.

### Operators that use Aggregate Functions
1. `Agg`


| AggFunction             | Return Type |
| ----------------------- | ----------- |
| `pl.count(BOOLEAN)`     | `INT64`     |
| `pl.count(INT64)`       | `INT64`     |
| `pl.count(FLOAT64)`     | `INT64`     |
| `pl.count(STRING)`      | `INT64`     |
| `pl.max(INT64)`         | `INT64`     |
| `pl.max(FLOAT64)`       | `FLOAT64`   |
| `pl.mean(BOOLEAN)`      | `FLOAT64`   |
| `pl.mean(INT64)`        | `FLOAT64`   |
| `pl.mean(FLOAT64)`      | `FLOAT64`   |
| `pl.min(INT64)`         | `INT64`     |
| `pl.min(FLOAT64)`       | `FLOAT64`   |
| `pl.quantiles(INT64)`   | `STRING`    |
| `pl.quantiles(FLOAT64)` | `STRING`    |
| `pl.sum(INT64)`         | `INT64`     |
| `pl.sum(FLOAT64)`       | `FLOAT64`   |