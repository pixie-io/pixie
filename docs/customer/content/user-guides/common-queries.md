---
title: "Common Queries"
metaTitle: "User Guides | Pixie"
metaDescription: "Pixie is ..."
---

The Pixie Console includes a list of query templates you can use to start analyzing data being collected by Pixie. 

A few of those sample queries are listed below:

#### Sample HTTP Data

```
t1 = dataframe(table='http_events', select=['time_', 'remote_addr', 'remote_port', 'http_resp_status', 'http_resp_message', 'http_resp_body', 'http_resp_latency_ns']).range(start='-30s')
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t2 = t1.limit(rows=100).result(name='resp_table')
```

#### Filter HTTP Spans by Specific Service

```
t1 = dataframe(table='http_events', select=['time_', 'remote_addr', 'remote_port', 'http_resp_status', 'http_resp_message', 'http_resp_body', 'http_resp_latency_ns']).range(start='-30s')
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t2 = t1.filter(fn=lambda r: r.remote_addr == <Add IP Address>).limit(rows=100).result(name='resp_table')
```

#### Count HTTP Spans by Service
```
t1 = dataframe(table='http_events', select=['time_', 'remote_addr', 'remote_port', 'http_resp_status', 'http_resp_message', 'http_resp_body', 'http_resp_latency_ns']).range(start='-30s')
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t2 = t1.agg(by=lambda r: [r.remote_addr], fn=lambda r: {'count': pl.count(r.remote_addr)})
t3 = t2.result(name='resp_table')
```


#### Data Exfiltration Check

```t1 = dataframe(table='http_events', select=['time_', 'remote_addr', 'remote_port', 'http_resp_status', 'http_resp_message', 'http_resp_body', 'http_resp_latency_ns']).range(start='-30s')
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t1['Alert'] = t1['remote_addr'] == '<Add IP Address>'
t2 = t1.result(name='resp_table')
```
#### Sample CPU Stats

```
t1 = dataframe(table='process_stats', select=['time_', 'upid', 'major_faults', 'minor_faults', 'cpu_utime_ns', 'cpu_ktime_ns',
                                         'num_threads', 'vsize_bytes', 'rss_bytes', 'rchar_bytes', 'wchar_bytes', 'read_bytes', 'write_bytes'] ).range(start='-30s')
t2 = t1.limit(rows=100).result(name='stats')
```

#### Sample Network Stats

```
t1 = dataframe(table='network_stats', select=['time_', 'pod_id', 'rx_bytes', 'rx_packets', 'rx_errors',
                                         'rx_drops', 'tx_bytes', 'tx_packets', 'tx_errors', 'tx_drops']).range(start='-30s')
t2 = t1.limit(rows=100).result(name='stats')
```