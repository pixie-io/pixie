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

package goplanner_test

// TODO move to some file
// TODO add px/pod.
const schemaPB = `
relation_map {
	key: "conn_stats"
	value {
		columns {
			column_name: "time_"
			column_type: TIME64NS
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "upid"
			column_type: UINT128
			column_semantic_type: ST_UPID
		}
		columns {
			column_name: "remote_addr"
			column_type: STRING
			column_semantic_type: ST_IP_ADDRESS
		}
		columns {
			column_name: "remote_port"
			column_type: INT64
			column_semantic_type: ST_PORT
		}
		columns {
			column_name: "local_addr"
			column_type: STRING
			column_semantic_type: ST_IP_ADDRESS
		}
		columns {
			column_name: "local_port"
			column_type: INT64
			column_semantic_type: ST_PORT
		}
		columns {
			column_name: "protocol"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "role"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "conn_open"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "conn_close"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "conn_active"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "bytes_sent"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "bytes_recv"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
	}
}
relation_map {
	key: "cql_events"
	value {
		columns {
			column_name: "time_"
			column_type: TIME64NS
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "upid"
			column_type: UINT128
			column_semantic_type: ST_UPID
		}
		columns {
			column_name: "remote_addr"
			column_type: STRING
			column_semantic_type: ST_IP_ADDRESS
		}
		columns {
			column_name: "remote_port"
			column_type: INT64
			column_semantic_type: ST_PORT
		}
		columns {
			column_name: "trace_role"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req_op"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req_body"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_op"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_body"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "latency_ns"
			column_type: INT64
			column_semantic_type: ST_DURATION_NS
		}
	}
}
relation_map {
	key: "http_events"
	value {
		columns {
			column_name: "time_"
			column_type: TIME64NS
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "upid"
			column_type: UINT128
			column_semantic_type: ST_UPID
		}
		columns {
			column_name: "remote_addr"
			column_type: STRING
			column_semantic_type: ST_IP_ADDRESS
		}
		columns {
			column_name: "remote_port"
			column_type: INT64
			column_semantic_type: ST_PORT
		}
		columns {
			column_name: "trace_role"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "major_version"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "minor_version"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "content_type"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req_headers"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req_method"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req_path"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req_body"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_headers"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_status"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_message"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_body"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_body_size"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_latency_ns"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
	}
}
relation_map {
	key: "jvm_stats"
	value {
		columns {
			column_name: "time_"
			column_type: TIME64NS
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "upid"
			column_type: UINT128
			column_semantic_type: ST_UPID
		}
		columns {
			column_name: "young_gc_time"
			column_type: INT64
			column_semantic_type: ST_DURATION_NS
		}
		columns {
			column_name: "full_gc_time"
			column_type: INT64
			column_semantic_type: ST_DURATION_NS
		}
		columns {
			column_name: "used_heap_size"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "total_heap_size"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "max_heap_size"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
	}
}
relation_map {
	key: "mysql_events"
	value {
		columns {
			column_name: "time_"
			column_type: TIME64NS
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "upid"
			column_type: UINT128
			column_semantic_type: ST_UPID
		}
		columns {
			column_name: "remote_addr"
			column_type: STRING
			column_semantic_type: ST_IP_ADDRESS
		}
		columns {
			column_name: "remote_port"
			column_type: INT64
			column_semantic_type: ST_PORT
		}
		columns {
			column_name: "trace_role"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req_cmd"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req_body"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_status"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp_body"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "latency_ns"
			column_type: INT64
			column_semantic_type: ST_DURATION_NS
		}
	}
}
relation_map {
	key: "network_stats"
	value {
		columns {
			column_name: "time_"
			column_type: TIME64NS
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "pod_id"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "rx_bytes"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "rx_packets"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "rx_errors"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "rx_drops"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "tx_bytes"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "tx_packets"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "tx_errors"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "tx_drops"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
	}
}
relation_map {
	key: "pgsql_events"
	value {
		columns {
			column_name: "time_"
			column_type: TIME64NS
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "upid"
			column_type: UINT128
			column_semantic_type: ST_UPID
		}
		columns {
			column_name: "remote_addr"
			column_type: STRING
			column_semantic_type: ST_IP_ADDRESS
		}
		columns {
			column_name: "remote_port"
			column_type: INT64
			column_semantic_type: ST_PORT
		}
		columns {
			column_name: "trace_role"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "req"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "resp"
			column_type: STRING
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "latency_ns"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
	}
}
relation_map {
	key: "process_stats"
	value {
		columns {
			column_name: "time_"
			column_type: TIME64NS
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "upid"
			column_type: UINT128
			column_semantic_type: ST_UPID
		}
		columns {
			column_name: "major_faults"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "minor_faults"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "cpu_utime_ns"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "cpu_ktime_ns"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "num_threads"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "vsize_bytes"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "rss_bytes"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "rchar_bytes"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "wchar_bytes"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "read_bytes"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
		columns {
			column_name: "write_bytes"
			column_type: INT64
			column_semantic_type: ST_NONE
		}
	}
}
`
