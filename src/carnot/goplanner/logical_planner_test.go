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

import (
	"os"
	"testing"

	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/carnot/goplanner"
	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	logical "px.dev/pixie/src/carnot/planner/dynamic_tracing/ir/logicalpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/udfspb"
	"px.dev/pixie/src/common/base/statuspb"
	funcs "px.dev/pixie/src/vizier/funcs/go"
)

const plannerStatePBStr = `
distributed_state {
	carnot_info {
		query_broker_address: "pem1"
		agent_id {
			high_bits: 0x0000000100000000
			low_bits: 0x0000000000000001
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
			high_bits: 0x0000000100000000
			low_bits: 0x0000000000000002
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
			high_bits: 0x0000000100000000
			low_bits: 0x0000000000000004
		}
		grpc_address: "1111"
		has_grpc_server: true
		has_data_store: false
		processes_data: true
		accepts_remote_sources: true
		asid: 456
	}
	schema_info {
		name: "table1"
		relation {
			columns {
				column_name: "time_"
				column_type: TIME64NS
				column_semantic_type: ST_NONE
			}
			columns {
				column_name: "cpu_cycles"
				column_type: INT64
				column_semantic_type: ST_NONE
			}
			columns {
				column_name: "upid"
				column_type: UINT128
				column_semantic_type: ST_NONE
			}
		}
		agent_list {
			high_bits: 0x0000000100000000
			low_bits: 0x0000000000000001
		}
		agent_list {
			high_bits: 0x0000000100000000
			low_bits: 0x0000000000000002
		}
		agent_list {
			high_bits: 0x0000000100000000
			low_bits: 0x0000000000000003
		}
	}
}`

// TestPlanner_Simple makes sure that we can actually pass in all the info needed
// to create a PlannerState and can successfully compile to an expected result.
func TestPlanner_Simple(t *testing.T) {
	// Create the compiler.
	var udfInfoPb udfspb.UDFInfo
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	require.NoError(t, err)

	err = proto.Unmarshal(b, &udfInfoPb)
	require.NoError(t, err)

	c, err := goplanner.New(&udfInfoPb)
	require.NoError(t, err)
	defer c.Free()

	// Pass the relation proto, table and query to the compilation.
	query := "import px\ndf = px.DataFrame(table='table1')\npx.display(df, 'out')"
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	err = proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	require.NoError(t, err)

	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr:            query,
		LogicalPlannerState: plannerStatePB,
	}
	plannerResultPB, err := c.Plan(queryRequestPB)

	if err != nil {
		log.Fatalln("Failed to plan:", err)
		os.Exit(1)
	}

	status := plannerResultPB.Status
	assert.Equal(t, status.ErrCode, statuspb.OK)

	planPB := plannerResultPB.Plan

	kelvinPlan := planPB.QbAddressToPlan["kelvin"]
	kelvinGRPCSourceParentNode1 := kelvinPlan.Nodes[0].Nodes[0]
	kelvinGRPCSourceParentNode2 := kelvinPlan.Nodes[0].Nodes[1]
	kelvinGRPCSource1 := kelvinGRPCSourceParentNode1.Op.GetGRPCSourceOp()
	kelvinGRPCSource2 := kelvinGRPCSourceParentNode2.Op.GetGRPCSourceOp()
	require.NotNil(t, kelvinGRPCSource1)
	require.NotNil(t, kelvinGRPCSource2)

	assert.Equal(t, 3, len(planPB.Dag.GetNodes()))
	pem1Plan := planPB.QbAddressToPlan["pem1"]
	pem1MemSrc1 := pem1Plan.Nodes[0].Nodes[0].Op.GetMemSourceOp()
	require.NotNil(t, pem1MemSrc1)

	pem2Plan := planPB.QbAddressToPlan["pem2"]
	pem2MemSrc1 := pem2Plan.Nodes[0].Nodes[0].Op.GetMemSourceOp()
	require.NotNil(t, pem2MemSrc1)
	pem1GRPCSink := pem1Plan.Nodes[0].Nodes[len(pem1Plan.Nodes[0].Nodes)-1].Op.GetGRPCSinkOp()
	require.NotNil(t, pem1GRPCSink)
	pem2GRPCSink := pem2Plan.Nodes[0].Nodes[len(pem2Plan.Nodes[0].Nodes)-1].Op.GetGRPCSinkOp()
	require.NotNil(t, pem2GRPCSink)
	assert.Equal(t, pem1GRPCSink.Address, "1111")
	assert.Equal(t, pem2GRPCSink.Address, "1111")
	assert.ElementsMatch(t,
		[]uint64{pem1GRPCSink.GetGRPCSourceID(), pem2GRPCSink.GetGRPCSourceID()},
		[]uint64{kelvinGRPCSourceParentNode2.Id, kelvinGRPCSourceParentNode1.Id})
}

func TestPlanner_MissingTable(t *testing.T) {
	// Create the compiler.
	c, err := goplanner.New(&udfspb.UDFInfo{})
	require.NoError(t, err)
	defer c.Free()

	// Pass the relation proto, table and query to the compilation.
	query := "import px\ndf = px.DataFrame(table='bad_table')\npx.display(df, 'out')"
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	err = proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	require.NoError(t, err)

	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr:            query,
		LogicalPlannerState: plannerStatePB,
	}
	plannerResultPB, err := c.Plan(queryRequestPB)

	if err != nil {
		log.Fatalln("Failed to plan:", err)
		os.Exit(1)
	}

	status := plannerResultPB.Status
	assert.NotEqual(t, status.ErrCode, statuspb.OK)
	var errorPB compilerpb.CompilerErrorGroup
	err = goplanner.GetCompilerErrorContext(status, &errorPB)

	require.NoError(t, err)

	require.Equal(t, 1, len(errorPB.Errors))
	compilerError := errorPB.Errors[0]
	lineColError := compilerError.GetLineColError()
	require.NotNil(t, lineColError)
}

// This test makes sure the goplanner actually works if an empty string is passed in.
func TestPlanner_EmptyString(t *testing.T) {
	// Create the compiler.
	c, err := goplanner.New(&udfspb.UDFInfo{})
	require.NoError(t, err)
	defer c.Free()

	// Empty string should yield a status error not a CHECK failure in cgo_export.
	query := ""
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	err = proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	require.NoError(t, err)

	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr:            query,
		LogicalPlannerState: plannerStatePB,
	}
	plannerResultPB, err := c.Plan(queryRequestPB)

	if err != nil {
		t.Fatal("Failed to plan:", err)
	}

	status := plannerResultPB.Status
	assert.NotEqual(t, status.ErrCode, statuspb.OK)
	assert.Regexp(t, "Query should not be empty", status.Msg)
}

const probePxl = `
import pxtrace
import px

@pxtrace.probe("MyFunc")
def probe_func():
    id = pxtrace.ArgExpr('id')
    return [{'id': id},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]

pxtrace.UpsertTracepoint('http_return',
                         "http_return_table",
                         probe_func,
                         px.uint128("123e4567-e89b-12d3-a456-426655440000"),
                         "5m")
`

const expectedDynamicTraceStr = `
name: "http_return"
ttl {
  seconds: 300
}
deployment_spec {
  upid {
    asid: 306070887 pid: 3902477011 ts_ns: 11841725277501915136
  }
}
programs {
  table_name: "http_return_table"
  spec {
    outputs {
      name: "http_return_table"
      fields: "id"
      fields: "err"
      fields: "latency"
    }
    probe {
      name: "http_return"
      tracepoint {
        symbol: "MyFunc"
      }
      args {
        id: "arg0"
        expr: "id"
      }
      ret_vals {
        id: "ret0"
        expr: "$0.a"
      }
      function_latency {
        id: "lat0"
      }
      output_actions {
        output_name: "http_return_table"
        variable_names: "arg0"
        variable_names: "ret0"
        variable_names: "lat0"
      }
    }
  }
}
`

// TestPlanner_CompileRequest makes sure that we can actually pass in all the info needed
// to create a PlannerState and can successfully compile to an expected result.
func TestPlanner_CompileRequest(t *testing.T) {
	// Create the compiler.
	var udfInfoPb udfspb.UDFInfo
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	require.NoError(t, err)

	err = proto.Unmarshal(b, &udfInfoPb)
	require.NoError(t, err)

	c, err := goplanner.New(&udfInfoPb)
	require.NoError(t, err)
	defer c.Free()

	// Pass the relation proto, table and query to the compilation.
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	err = proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	require.NoError(t, err)

	compileRequestPB := &plannerpb.CompileMutationsRequest{
		QueryStr:            probePxl,
		LogicalPlannerState: plannerStatePB,
	}
	compileMutationResponse, err := c.CompileMutations(compileRequestPB)
	require.NoError(t, err)

	status := compileMutationResponse.Status
	require.Equal(t, status.ErrCode, statuspb.OK)

	var expectedDynamicTracePb logical.TracepointDeployment
	err = proto.UnmarshalText(expectedDynamicTraceStr, &expectedDynamicTracePb)
	require.NoError(t, err)

	assert.Equal(t, 1, len(compileMutationResponse.Mutations))
	assert.Equal(t, &expectedDynamicTracePb, compileMutationResponse.Mutations[0].GetTrace())
}

func TestPlanner_GenerateOTelScriptPropagates(t *testing.T) {
	// Create the compiler.
	var udfInfoPb udfspb.UDFInfo
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	require.NoError(t, err)

	err = proto.Unmarshal(b, &udfInfoPb)
	require.NoError(t, err)

	c, err := goplanner.New(&udfInfoPb)
	require.NoError(t, err)
	defer c.Free()

	// Pass the relation proto, table and query to the compilation.
	query := `import px
df = px.DataFrame(table='table1')
df.service = df.ctx['service']
px.display(df[['time_', 'service', 'cpu_cycles']], 'out')`
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	err = proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	require.NoError(t, err)

	otelResponse, err := c.GenerateOTelScript(&plannerpb.GenerateOTelScriptRequest{
		PxlScript:           query,
		LogicalPlannerState: plannerStatePB,
	})
	require.NoError(t, err)
	require.Equal(t, statuspb.OK, otelResponse.Status.ErrCode)
	require.Equal(t, `import px
df = px.DataFrame('table1', start_time=px.plugin.start_time, end_time=px.plugin.end_time)
df.service = df.ctx['service']
px.display(df[['time_', 'service', 'cpu_cycles']], 'out')

otel_df = df[['time_', 'service', 'cpu_cycles']]
px.export(otel_df, px.otel.Data(
  resource={
    'out.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='out.cpu_cycles',
      description='',
      value=otel_df.cpu_cycles,
    )
  ]
))`, otelResponse.OTelScript)
}
