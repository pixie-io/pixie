---
title: "Table Schemas"
metaTitle: "Query Language | Pixie"
metaDescription: "These are tables that Pixie users can reference"
---

The following tables are queriable via the Pixie Console: 


#### TABLE: ```HTTP_EVENTS```

| Field                | DataType | PatternType    |
| -------------------- | -------- | -------------- |
| time_                | TIME64NS | METRIC_COUNTER |
| upid                 | UINT128  | GENERAL        |
| remote_addr          | STRING   | GENERAL        |
| remote_port          | INT64    | GENERAL        |
| http_major_version   | INT64    | GENERAL_ENUM   |
| http_minor_version   | INT64    | GENERAL_ENUM   |
| http_content_type    | INT64    | GENERAL_ENUM   |
| http_req_headers     | STRING   | STRUCTURED     |
| http_req_method      | STRING   | GENERAL_ENUM   |
| http_req_path        | STRING   | STRUCTURED     |
| http_req_body        | STRING   | STRUCTURED     |
| http_resp_headers    | STRING   | STRUCTURED     |
| http_resp_status     | INT64    | GENERAL_ENUM   |
| http_resp_message    | STRING   | STRUCTURED     |
| http_resp_body       | STRING   | STRUCTURED     |
| http_resp_latency_ns | INT64    | METRIC_GAUGE   |


#### TABLE: ```PROCESS_STATS```

| Field        | DataType | PatternType    |
| ------------ | -------- | -------------- |
| time_        | TIME64NS | METRIC_COUNTER |
| upid         | UINT128  | GENERAL        |
| major_faults | INT64    | METRIC_COUNTER |
| minor_faults | INT64    | METRIC_COUNTER |
| cpu_utime_ns | INT64    | METRIC_COUNTER |
| cpu_ktime_ns | INT64    | METRIC_COUNTER |
| num_threads  | INT64    | METRIC_GAUGE   |
| vsize_bytes  | INT64    | METRIC_GAUGE   |
| rss_bytes    | INT64    | METRIC_GAUGE   |
| rchar_bytes  | INT64    | METRIC_GAUGE   |
| wchar_bytes  | INT64    | METRIC_GAUGE   |
| read_bytes   | INT64    | METRIC_GAUGE   |
| write_bytes  | INT64    | METRIC_GAUGE   |


#### TABLE: ```NETWORK_STATS```

| Field      | DataType | PatternTyepe   |
| ---------- | -------- | -------------- |
| time_      | TIME64NS | METRIC_COUNTER |
| pod_id     | STRING   | GENERAL        |
| rx_bytes   | INT64    | METRIC_COUNTER |
| rx_packets | INT64    | METRIC_COUNTER |
| rx_errors  | INT64    | METRIC_COUNTER |
| rx_drops   | INT64    | METRIC_COUNTER |
| tx_bytes   | INT64    | METRIC_COUNTER |
| tx_packets | INT64    | METRIC_COUNTER |
| tx_errors  | INT64    | METRIC_COUNTER |
| tx_drops   | INT64    | METRIC_COUNTER |
