---
title: "Time in Queries"
metaTitle: "Pixie QL reference | Pixie"
metaDescription: "How to specify time in queries."
---

Pixie's Monitoring system keeps track of time as a 64 bit integer marking the number of nanoseconds since the Unix epoch - equivalent to the nanosecond representation of [Unix time](https://en.wikipedia.org/wiki/Unix_time). 

Because specifying nanoseconds since the Unix epoch is annoying, PixieQL has several alternatives to specify times and durations. 

1. Strings: `'-2m'`
2. Time functions: `plc.now() - plc.minutes(2)`
3. Plain old integer: `1552607213931245000` (Unix time in nanoseconds)




## String Time Specification

Strings are the most compact way to specify a time in the query.  Each string represents the time relative to "now", the time according to the compiler.

To filter all http requests from the past 30 seconds, you would specify the time with `-30s`:
```python
From(table='http_events').Range(start='-30s').Result(name='out')
```


All of the possible units are listed below.

| Unit         | Example | Time Function                      |
| ------------ | ------- | ---------------------------------- |
| microseconds | `-21us` | `plc.now() - plc.microseconds(21)` |
| milliseconds | `-13ms` | `plc.now() - plc.milliseconds(13)` |
| seconds      | `-8s`   | `plc.now() - plc.seconds(8)`       |
| minutes      | `-5m`   | `plc.now() - plc.minutes(5)`       |
| hours        | `-3h`   | `plc.now() - plc.hours(3)`         |
| days         | `-1d`   | `plc.now() - plc.days(1)`          |
| weeks        | `-1w`   | `plc.now() - plc.weeks(1)`         |


## Time Functions
Time functions can be used to specify more complicated time ranges. 

| Unit         | Time Function              |
| ------------ | -------------------------- |
| microseconds | `plc.microseconds(int us)` |
| milliseconds | `plc.milliseconds(int ms)` |
| seconds      | `plc.seconds(int s)`       |
| minutes      | `plc.minutes(int m)`       |
| hours        | `plc.hours(int h)`         |
| days         | `plc.days(int d)`          |
| weeks        | `plc.weeks(int w)`         |

Additionally, you can access the current time with `plc.now()`


To filter all http requests from the past 30 seconds, you would specify the time with `plc.now() - plc.minutes(30)`:
```python
From(table='http_events').Range(start=plc.now() - plc.minutes(30)).Result(name='out')
```