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

package main

import (
	"os"
	"path"
	"path/filepath"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/carnot/goplanner"
	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/carnot/queryresultspb"
	"px.dev/pixie/src/carnot/udfspb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/script"
	funcs "px.dev/pixie/src/vizier/funcs/go"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
)

const plannerStatePBStr = `
distributed_state {
	carnot_info {
		query_broker_address: "pem1"
		agent_id {
			data: "00000001-0000-0000-0000-000000000001"
		}
		has_grpc_server: false
		has_data_store: true
		processes_data: true
		accepts_remote_sources: false
		asid: 123
		table_info {
			table: "table"
		}
	}
	carnot_info {
		query_broker_address: "pem2"
		agent_id {
			data: "00000001-0000-0000-0000-000000000002"
		}
		has_grpc_server: false
		has_data_store: true
		processes_data: true
		accepts_remote_sources: false
		asid: 789
		table_info {
			table: "table"
		}
	}
	carnot_info {
		query_broker_address: "kelvin"
		agent_id {
			data: "00000001-0000-0000-0000-000000000004"
		}
		grpc_address: "1111"
		has_grpc_server: true
		has_data_store: false
		processes_data: true
		accepts_remote_sources: true
		asid: 456
	}
	schema_info: {
		name: "conn_stats"
		relation: {
			columns: {
				column_name: "time_"
				column_type: TIME64NS
				column_desc: "Timestamp when the data record was collected."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "upid"
				column_type: UINT128
				column_desc: "An opaque numeric ID that globally identify a running process inside the cluster."
				column_semantic_type: ST_UPID
			}
			columns: {
				column_name: "remote_addr"
				column_type: STRING
				column_desc: "IP address of the remote endpoint."
				column_semantic_type: ST_IP_ADDRESS
			}
			columns: {
				column_name: "remote_port"
				column_type: INT64
				column_desc: "Port of the remote endpoint."
				column_semantic_type: ST_PORT
			}
			columns: {
				column_name: "protocol"
				column_type: INT64
				column_desc: "The protocol of the traffic on the connections."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "role"
				column_type: INT64
				column_desc: "The role of the process that owns the connections."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "conn_open"
				column_type: INT64
				column_desc: "The number of connections opened since the beginning of tracing."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "conn_close"
				column_type: INT64
				column_desc: "The number of connections closed since the beginning of tracing."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "conn_active"
				column_type: INT64
				column_desc: "The number of active connections"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "bytes_sent"
				column_type: INT64
				column_desc: "The number of bytes sent to the remote endpoint(s)."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "bytes_recv"
				column_type: INT64
				column_desc: "The number of bytes received from the remote endpoint(s)."
				column_semantic_type: ST_NONE
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}
	}
	schema_info: {
		name: "cql_events"
		relation: {
			columns: {
				column_name: "time_"
				column_type: TIME64NS
				column_desc: "Timestamp when the data record was collected."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "upid"
				column_type: UINT128
				column_desc: "An opaque numeric ID that globally identify a running process inside the cluster."
				column_semantic_type: ST_UPID
			}
			columns: {
				column_name: "remote_addr"
				column_type: STRING
				column_desc: "IP address of the remote endpoint."
				column_semantic_type: ST_IP_ADDRESS
			}
			columns: {
				column_name: "remote_port"
				column_type: INT64
				column_desc: "Port of the remote endpoint."
				column_semantic_type: ST_PORT
			}
			columns: {
				column_name: "trace_role"
				column_type: INT64
				column_desc: "Side (client-or-server) where traffic was traced"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req_op"
				column_type: INT64
				column_desc: "Request opcode"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req_body"
				column_type: STRING
				column_desc: "Request body"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_op"
				column_type: INT64
				column_desc: "Response opcode"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_body"
				column_type: STRING
				column_desc: "Request body"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "latency_ns"
				column_type: INT64
				column_desc: "Request-response latency in nanoseconds"
				column_semantic_type: ST_DURATION_NS
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}
	}
	schema_info: {
		name: "http_events"
		relation: {
			columns: {
				column_name: "time_"
				column_type: TIME64NS
				column_desc: "Timestamp when the data record was collected."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "upid"
				column_type: UINT128
				column_desc: "An opaque numeric ID that globally identify a running process inside the cluster."
				column_semantic_type: ST_UPID
			}
			columns: {
				column_name: "remote_addr"
				column_type: STRING
				column_desc: "IP address of the remote endpoint."
				column_semantic_type: ST_IP_ADDRESS
			}
			columns: {
				column_name: "remote_port"
				column_type: INT64
				column_desc: "Port of the remote endpoint."
				column_semantic_type: ST_PORT
			}
			columns: {
				column_name: "trace_role"
				column_type: INT64
				column_desc: "Side (client-or-server) where traffic was traced"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "major_version"
				column_type: INT64
				column_desc: "HTTP major version, can be 1 or 2"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "minor_version"
				column_type: INT64
				column_desc: "HTTP minor version, HTTP1 uses 1, HTTP2 set this value to 0"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "content_type"
				column_type: INT64
				column_desc: "Type of the HTTP payload, can be JSON or protobuf"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req_headers"
				column_type: STRING
				column_desc: "Request headers in JSON format"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req_method"
				column_type: STRING
				column_desc: "HTTP request method (e.g. GET, POST, ...)"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req_path"
				column_type: STRING
				column_desc: "Request path"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req_body"
				column_type: STRING
				column_desc: "Request body in JSON format"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_headers"
				column_type: STRING
				column_desc: "Response headers in JSON format"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_status"
				column_type: INT64
				column_desc: "HTTP response status code"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_message"
				column_type: STRING
				column_desc: "HTTP response status text (e.g. OK, Not Found, ...)"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_body"
				column_type: STRING
				column_desc: "Response body in JSON format"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_body_size"
				column_type: INT64
				column_desc: "Response body size (before any truncation)"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_latency_ns"
				column_type: INT64
				column_desc: "Request-response latency in nanoseconds"
				column_semantic_type: ST_NONE
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}
	}
	schema_info: {
		name: "jvm_stats"
		relation: {
			columns: {
				column_name: "time_"
				column_type: TIME64NS
				column_desc: "Timestamp when the data record was collected."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "upid"
				column_type: UINT128
				column_desc: "An opaque numeric ID that globally identify a running process inside the cluster."
				column_semantic_type: ST_UPID
			}
			columns: {
				column_name: "young_gc_time"
				column_type: INT64
				column_desc: "Young generation garbage collection time in nanoseconds."
				column_semantic_type: ST_DURATION_NS
			}
			columns: {
				column_name: "full_gc_time"
				column_type: INT64
				column_desc: "Full garbage collection time in nanoseconds."
				column_semantic_type: ST_DURATION_NS
			}
			columns: {
				column_name: "used_heap_size"
				column_type: INT64
				column_desc: "Used heap size in bytes."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "total_heap_size"
				column_type: INT64
				column_desc: "Total heap size in bytes."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "max_heap_size"
				column_type: INT64
				column_desc: "Maximal heap capacity in bytes."
				column_semantic_type: ST_NONE
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}
	}
	schema_info: {
		name: "mysql_events"
		relation: {
			columns: {
				column_name: "time_"
				column_type: TIME64NS
				column_desc: "Timestamp when the data record was collected."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "upid"
				column_type: UINT128
				column_desc: "An opaque numeric ID that globally identify a running process inside the cluster."
				column_semantic_type: ST_UPID
			}
			columns: {
				column_name: "remote_addr"
				column_type: STRING
				column_desc: "IP address of the remote endpoint."
				column_semantic_type: ST_IP_ADDRESS
			}
			columns: {
				column_name: "remote_port"
				column_type: INT64
				column_desc: "Port of the remote endpoint."
				column_semantic_type: ST_PORT
			}
			columns: {
				column_name: "trace_role"
				column_type: INT64
				column_desc: "Side (client-or-server) where traffic was traced"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req_cmd"
				column_type: INT64
				column_desc: "MySQL request command"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req_body"
				column_type: STRING
				column_desc: "MySQL request body"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_status"
				column_type: INT64
				column_desc: "MySQL response status code"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp_body"
				column_type: STRING
				column_desc: "MySQL response body"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "latency_ns"
				column_type: INT64
				column_desc: "Request-response latency in nanoseconds"
				column_semantic_type: ST_DURATION_NS
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}

	}
	schema_info: {
		name: "network_stats"
		relation: {
			columns: {
				column_name: "time_"
				column_type: TIME64NS
				column_desc: "Timestamp when the data record was collected."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "pod_id"
				column_type: STRING
				column_desc: "The ID of the pod"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "rx_bytes"
				column_type: INT64
				column_desc: "Received network traffic in bytes of the pod"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "rx_packets"
				column_type: INT64
				column_desc: "Number of received network packets of the pod"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "rx_errors"
				column_type: INT64
				column_desc: "Number of network receive errors of the pod"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "rx_drops"
				column_type: INT64
				column_desc: "Number of dropped network packets being received of the pod"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "tx_bytes"
				column_type: INT64
				column_desc: "Transmitted network traffic of the pod"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "tx_packets"
				column_type: INT64
				column_desc: "Number of transmitted network packets of the pod"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "tx_errors"
				column_type: INT64
				column_desc: "Number of network transmit errors of the pod"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "tx_drops"
				column_type: INT64
				column_desc: "Number of dropped network packets being transmitted of the pod"
				column_semantic_type: ST_NONE
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}
	}
	schema_info: {
		name: "pgsql_events"
		relation: {
			columns: {
				column_name: "time_"
				column_type: TIME64NS
				column_desc: "Timestamp when the data record was collected."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "upid"
				column_type: UINT128
				column_desc: "An opaque numeric ID that globally identify a running process inside the cluster."
				column_semantic_type: ST_UPID
			}
			columns: {
				column_name: "remote_addr"
				column_type: STRING
				column_desc: "IP address of the remote endpoint."
				column_semantic_type: ST_IP_ADDRESS
			}
			columns: {
				column_name: "remote_port"
				column_type: INT64
				column_desc: "Port of the remote endpoint."
				column_semantic_type: ST_PORT
			}
			columns: {
				column_name: "trace_role"
				column_type: INT64
				column_desc: "Side (client-or-server) where traffic was traced"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "req"
				column_type: STRING
				column_desc: "PostgreSQL request body"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "resp"
				column_type: STRING
				column_desc: "PostgreSQL response body"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "latency_ns"
				column_type: INT64
				column_desc: "Request-response latency in nanoseconds"
				column_semantic_type: ST_NONE
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}
	}
	schema_info: {
		name: "process_stats"
		relation: {
			columns: {
				column_name: "time_"
				column_type: TIME64NS
				column_desc: "Timestamp when the data record was collected."
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "upid"
				column_type: UINT128
				column_desc: "An opaque numeric ID that globally identify a running process inside the cluster."
				column_semantic_type: ST_UPID
			}
			columns: {
				column_name: "major_faults"
				column_type: INT64
				column_desc: "Number of major page faults"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "minor_faults"
				column_type: INT64
				column_desc: "Number of minor page faults"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "cpu_utime_ns"
				column_type: INT64
				column_desc: "Time spent on user space by the process"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "cpu_ktime_ns"
				column_type: INT64
				column_desc: "Time spent on kernel by the process"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "num_threads"
				column_type: INT64
				column_desc: "Number of threads of the process"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "vsize_bytes"
				column_type: INT64
				column_desc: "Virtual memory size in bytes of the process"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "rss_bytes"
				column_type: INT64
				column_desc: "Resident memory size in bytes of the process"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "rchar_bytes"
				column_type: INT64
				column_desc: "IO reads in bytes of the process"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "wchar_bytes"
				column_type: INT64
				column_desc: "IO writes in bytes of the process"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "read_bytes"
				column_type: INT64
				column_desc: "IO reads actually go to storage layer in bytes of the process"
				column_semantic_type: ST_NONE
			}
			columns: {
				column_name: "write_bytes"
				column_type: INT64
				column_desc: "IO writes actually go to storage layer in bytes of the process"
				column_semantic_type: ST_NONE
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}
	}
}`

const mustPEM = `
import px

base = px.DataFrame('http_events')
base.service = base.upid
df1 = base
#df1 = df1[px.contains(df1.ctx['service'], "service")]

df2 = base
#df2.blah = df2.resp_status == 200
# df2.pod = df2.ctx['pod']

px.display(df1.groupby('service').agg(count=('resp_status', px.count)))
px.display(df2.groupby('resp_status').agg(count=('resp_status', px.count)))
# px.display(df2)

`

func convertExecFuncs(inputFuncs []*vizierpb.ExecuteScriptRequest_FuncToExecute) []*plannerpb.FuncToExecute {
	funcs := make([]*plannerpb.FuncToExecute, len(inputFuncs))
	for i, f := range inputFuncs {
		args := make([]*plannerpb.FuncToExecute_ArgValue, len(f.ArgValues))
		for j, arg := range f.ArgValues {
			args[j] = &plannerpb.FuncToExecute_ArgValue{
				Name:  arg.Name,
				Value: arg.Value,
			}
		}
		funcs[i] = &plannerpb.FuncToExecute{
			FuncName:          f.FuncName,
			ArgValues:         args,
			OutputTablePrefix: f.OutputTablePrefix,
		}
	}
	return funcs
}

func main() {
	var readScriptFromDir = true
	var scriptDir = "/home/philkuz/library/pixie/pxl_scripts/px/cluster/"
	// Create the compiler.
	var udfInfoPb udfspb.UDFInfo
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	if err != nil {
		log.Fatalf("%v", err)
	}
	err = proto.Unmarshal(b, &udfInfoPb)
	if err != nil {
		log.Fatalf("%v", err)
	}
	c, err := goplanner.New(&udfInfoPb)
	if err != nil {
		log.Fatalf("%v", err)
	}
	defer c.Free()
	// Pass the relation proto, table and query to the compilation.
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	err = proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	if err != nil {
		log.WithError(err).Fatal("Failed to unmarshal text")
	}
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: mustPEM,
	}
	if readScriptFromDir {
		visPath := path.Join(scriptDir, "vis.json")
		visRaw, err := os.ReadFile(visPath)
		if err != nil {
			log.WithError(err).Fatalf("Loading vis.json failed")
		}

		matches, err := filepath.Glob(path.Join(scriptDir, "*.pxl"))
		if err != nil {
			log.WithError(err).Fatalf("Finding pxl file failed")
		}
		if len(matches) != 1 {
			log.Fatalf("Expected 1 pxl file, found %d in %s", len(matches), scriptDir)
		}

		query, err := os.ReadFile(matches[0])
		if err != nil {
			log.WithError(err).Fatalf("Loading pxl failed")
		}

		vs, err := script.ParseVisSpec(string(visRaw))
		if err != nil {
			log.WithError(err).Fatal("Failed to parse visspec")
		}
		visScript := &script.ExecutableScript{
			ScriptString: string(query),
			Vis:          vs,
		}
		execFuncs, err := vizier.GetFuncsToExecute(visScript)
		if err != nil {
			log.WithError(err).Fatalf("Parse exec funcs failed")
		}

		queryRequestPB = &plannerpb.QueryRequest{
			QueryStr:  string(query),
			ExecFuncs: convertExecFuncs(execFuncs),
		}
	}
	// Hack because the agent names in test are human readable but not the case in the final.
	for _, agent := range plannerStatePB.DistributedState.CarnotInfo {
		id := utils.UUIDFromProtoOrNil(agent.AgentID)
		agent.QueryBrokerAddress = id.String()
	}
	plannerResultPB, err := c.Plan(queryRequestPB)

	if err != nil {
		log.Fatalf("Failed to plan: %v", err)
	}

	status := plannerResultPB.Status
	if status.ErrCode != statuspb.OK {
		var errorPB compilerpb.CompilerErrorGroup
		err = goplanner.GetCompilerErrorContext(status, &errorPB)
		if err != nil {
			log.Fatalf("%v", err)
		}
		log.Infof("%v", status)
		log.Fatalf("%v", errorPB)
	}

	planPB := plannerResultPB.Plan
	planMap := make(map[uuid.UUID]*planpb.Plan)
	for carnotID, agentPlan := range planPB.QbAddressToPlan {
		u, err := uuid.FromString(carnotID)
		if err != nil {
			log.Fatalf("Couldn't parse uuid from agent id \"%s\"", carnotID)
		}
		planMap[u] = agentPlan
	}

	stats := &[]*queryresultspb.AgentExecutionStats{}
	planStr, _ := controllers.GetQueryPlanAsDotString(planPB, planMap, stats)
	log.Infof("%s", planStr)

	path := "/tmp/subgraph.dot"
	// check if file exists
	_, err = os.Stat(path)

	// create file if not exists
	if os.IsNotExist(err) {
		f, err := os.Create(path)
		if err != nil {
			log.WithError(err).Fatalf("failed to create")
		}
		defer f.Close()
	}

	f, err := os.OpenFile(path, os.O_RDWR, 0644)
	if err != nil {
		log.WithError(err).Fatalf("failed to open")
	}
	defer f.Close()

	_, err = f.WriteString(planStr)
	if err != nil {
		log.WithError(err).Fatalf("failed to write")
	}
}
