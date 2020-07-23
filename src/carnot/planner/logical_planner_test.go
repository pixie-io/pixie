package logicalplanner_test

import (
	"os"
	"testing"

	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/shared/scriptspb"

	"pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	logical "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir"

	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	logicalplanner "pixielabs.ai/pixielabs/src/carnot/planner"
	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/udfspb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	funcs "pixielabs.ai/pixielabs/src/vizier/funcs/export"
)

const plannerStatePBStr = `
schema {
  relation_map {
    key: "table1"
    value {
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
  }
}
distributed_state {
  carnot_info {
    query_broker_address: "agent1"
    has_data_store: true
    processes_data: true
    table_info {
      table: "table1"
      tabletization_key: "upid"
      tablets: "1"
      tablets: "2"
    }
  }
  carnot_info {
    query_broker_address: "agent2"
    has_data_store: true
    processes_data: true
    table_info {
      table: "table1"
      tabletization_key: "upid"
      tablets: "3"
      tablets: "4"
    }
	}
	carnot_info{
		query_broker_address: "kelvin"
		grpc_address: "1111"
		has_grpc_server: true
		has_data_store: false
		processes_data: true
		accepts_remote_sources: true
	}
}`

// TestPlanner_Simple makes sure that we can actually pass in all the info needed
// to create a PlannerState and can successfully compile to an expected result.
func TestPlanner_Simple(t *testing.T) {
	// Create the compiler.
	var udfInfoPb udfspb.UDFInfo
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	if !assert.Nil(t, err) {
		t.FailNow()
	}
	err = proto.Unmarshal(b, &udfInfoPb)
	if !assert.Nil(t, err) {
		t.FailNow()
	}
	c := logicalplanner.New(&udfInfoPb)
	defer c.Free()
	// Pass the relation proto, table and query to the compilation.
	query := "import px\ndf = px.DataFrame(table='table1')\npx.display(df, 'out')"
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: query,
	}
	plannerResultPB, err := c.Plan(plannerStatePB, queryRequestPB)

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
	if !assert.NotNil(t, kelvinGRPCSource1) {
		t.FailNow()
	}
	if !assert.NotNil(t, kelvinGRPCSource2) {
		t.FailNow()
	}

	assert.Equal(t, 3, len(planPB.Dag.GetNodes()))
	agent1Plan := planPB.QbAddressToPlan["agent1"]
	agent1MemSrc1 := agent1Plan.Nodes[0].Nodes[0].Op.GetMemSourceOp()
	agent1MemSrc2 := agent1Plan.Nodes[0].Nodes[1].Op.GetMemSourceOp()
	if !assert.NotNil(t, agent1MemSrc1) {
		t.FailNow()
	}
	if !assert.NotNil(t, agent1MemSrc2) {
		t.FailNow()
	}

	assert.ElementsMatch(t, []string{agent1MemSrc1.Tablet, agent1MemSrc2.Tablet}, []string{"1", "2"})

	agent2Plan := planPB.QbAddressToPlan["agent2"]
	agent2MemSrc1 := agent2Plan.Nodes[0].Nodes[0].Op.GetMemSourceOp()
	agent2MemSrc2 := agent2Plan.Nodes[0].Nodes[1].Op.GetMemSourceOp()
	if !assert.NotNil(t, agent2MemSrc1) {
		t.FailNow()
	}
	if !assert.NotNil(t, agent2MemSrc2) {
		t.FailNow()
	}
	assert.ElementsMatch(t, []string{agent2MemSrc1.Tablet, agent2MemSrc2.Tablet}, []string{"3", "4"})
	agent1GRPCSink := agent1Plan.Nodes[0].Nodes[len(agent1Plan.Nodes[0].Nodes)-1].Op.GetGRPCSinkOp()
	if !assert.NotNil(t, agent1GRPCSink) {
		t.FailNow()
	}
	assert.Equal(t, agent1GRPCSink.Address, "1111")
	assert.Equal(t, agent1GRPCSink.DestinationId, kelvinGRPCSourceParentNode2.Id)
	agent2GRPCSink := agent2Plan.Nodes[0].Nodes[len(agent2Plan.Nodes[0].Nodes)-1].Op.GetGRPCSinkOp()
	if !assert.NotNil(t, agent2GRPCSink) {
		t.FailNow()
	}
	assert.Equal(t, agent2GRPCSink.Address, "1111")
	assert.Equal(t, agent2GRPCSink.DestinationId, kelvinGRPCSourceParentNode1.Id)
}

func TestPlanner_MissingTable(t *testing.T) {
	// Create the compiler.
	c := logicalplanner.New(&udfspb.UDFInfo{})
	defer c.Free()
	// Pass the relation proto, table and query to the compilation.
	query := "import px\ndf = px.DataFrame(table='bad_table')\npx.display(df, 'out')"
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: query,
	}
	plannerResultPB, err := c.Plan(plannerStatePB, queryRequestPB)

	if err != nil {
		log.Fatalln("Failed to plan:", err)
		os.Exit(1)
	}

	status := plannerResultPB.Status
	assert.NotEqual(t, status.ErrCode, statuspb.OK)
	var errorPB compilerpb.CompilerErrorGroup
	err = logicalplanner.GetCompilerErrorContext(status, &errorPB)

	if !assert.NoError(t, err) {
		t.FailNow()
	}

	if !assert.Equal(t, 1, len(errorPB.Errors)) {
		t.FailNow()
	}
	compilerError := errorPB.Errors[0]
	lineColError := compilerError.GetLineColError()
	if !assert.NotNil(t, lineColError) {
		t.FailNow()
	}

}

// This test makes sure the logicalplanner actually works if an empty string is passed in.
func TestPlanner_EmptyString(t *testing.T) {
	// Create the compiler.
	c := logicalplanner.New(&udfspb.UDFInfo{})
	defer c.Free()
	// Empty string should yield a status error not a CHECK failure in cgo_export.
	query := ""
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: query,
	}
	plannerResultPB, err := c.Plan(plannerStatePB, queryRequestPB)

	if err != nil {
		t.Fatal("Failed to plan:", err)
	}

	status := plannerResultPB.Status
	assert.NotEqual(t, status.ErrCode, statuspb.OK)
	assert.Regexp(t, "Query should not be empty", status.Msg)
}

const mainFuncArgsQuery = `
import px

def main(foo : str):
		queryDF = px.DataFrame(table='cpu', select=['cpu0'])
		queryDF['foo_flag'] = foo
		px.display(queryDF, 'map')
`
const mainFuncArgsPBStr = `
	args {
		data_type: STRING
		name: "foo"
    semantic_type: ST_NONE
	}
`

func TestPlanner_GetMainFuncArgsSpec(t *testing.T) {
	// Create the planner.
	c := logicalplanner.New(&udfspb.UDFInfo{})
	defer c.Free()
	// Note that the string can't be empty since the cgo interface treats an empty string
	// as an error
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: mainFuncArgsQuery,
	}
	getMainFuncArgsResultPB, err := c.GetMainFuncArgsSpec(queryRequestPB)

	if err != nil {
		log.Fatalln("Failed to get flags: ", err)
		t.FailNow()
	}

	status := getMainFuncArgsResultPB.Status
	if !assert.Equal(t, status.ErrCode, statuspb.OK) {
		var errorPB compilerpb.CompilerErrorGroup
		err = logicalplanner.GetCompilerErrorContext(status, &errorPB)
		if err != nil {
			t.Fatalf("error while getting compiler err context, %s", err)
		}
		t.Fatalf("Parsing caused error %s", errorPB)
	}

	var expectedMainFuncArgsPB scriptspb.FuncArgsSpec

	if err = proto.UnmarshalText(mainFuncArgsPBStr, &expectedMainFuncArgsPB); err != nil {
		log.Fatalf("Failed to unmarshal expected proto", err)
		t.FailNow()
	}

	assert.Equal(t, &expectedMainFuncArgsPB, getMainFuncArgsResultPB.MainFuncSpec)
}

func TestPlanner_GetMainFuncArgsSpec_BadQuery(t *testing.T) {
	// Create the compiler.
	c := logicalplanner.New(&udfspb.UDFInfo{})
	defer c.Free()
	// query doesn't have a main function so should throw error.
	query := "import px\npx.display(px.DataFrame('http_events'))"
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: query,
	}
	plannerResultPB, err := c.GetMainFuncArgsSpec(queryRequestPB)

	if err != nil {
		t.Fatal("Failed to plan:", err)
	}

	status := plannerResultPB.Status
	assert.NotEqual(t, status.ErrCode, statuspb.OK)
	assert.Regexp(t, "Could not find 'main' fn", status.Msg)

}

const visFuncsQuery = `
import px
@px.vis.vega("vega spec for f")
def f(start_time: px.Time, end_time: px.Time, svc: str):
  """Doc string for f"""
  return 1

@px.vis.vega("vega spec for g")
def g(a: int, b: float):
  """Doc string for g"""
  return 1
`

const expectedVisFuncsInfoPBStr = `
doc_string_map {
  key: "f"
  value: "Doc string for f"
}
doc_string_map {
  key: "g"
  value: "Doc string for g"
}
vis_spec_map {
  key: "f"
  value {
    vega_spec: "vega spec for f"
  }
}
vis_spec_map {
  key: "g"
  value {
    vega_spec: "vega spec for g"
  }
}
fn_args_map {
  key: "f"
  value {
    args {
      data_type: TIME64NS
      name: "start_time"
    	semantic_type: ST_NONE
    }
    args {
      data_type: TIME64NS
      name: "end_time"
    	semantic_type: ST_NONE
    }
    args {
      data_type: STRING
      name: "svc"
   		semantic_type: ST_NONE
    }
  }
}
fn_args_map {
  key: "g"
  value {
    args {
      data_type: INT64
			name: "a"
    	semantic_type: ST_NONE
    }
    args {
      data_type: FLOAT64
      name: "b"
    	semantic_type: ST_NONE
    }
  }
}
`

func TestPlanner_ExtractVisFuncsInfo(t *testing.T) {
	c := logicalplanner.New(&udfspb.UDFInfo{})
	defer c.Free()

	visFuncsResult, err := c.ExtractVisFuncsInfo(visFuncsQuery)

	if err != nil {
		log.Fatalln("Failed to get vis funcs info: ", err)
		t.FailNow()
	}

	status := visFuncsResult.Status
	assert.Equal(t, status.ErrCode, statuspb.OK)

	var expectedVisFuncsInfoPb scriptspb.VisFuncsInfo

	if err = proto.UnmarshalText(expectedVisFuncsInfoPBStr, &expectedVisFuncsInfoPb); err != nil {
		log.Fatalf("Failed to unmarshal expected proto", err)
		t.FailNow()
	}

	assert.Equal(t, &expectedVisFuncsInfoPb, visFuncsResult.Info)
}

const probePxl = `
import pxtrace
import px

@pxtrace.goprobe("MyFunc")
def probe_func():
		id = pxtrace.ArgExpr('id')
		return "http_return_table", [{'id': id},
						{'err': pxtrace.RetExpr('$0.a')},
						{'latency': pxtrace.FunctionLatency()}]

pxtrace.UpsertTrace('http_return',
										probe_func,
										px.uint128("123e4567-e89b-12d3-a456-426655440000"),
										"5m")
`

const expectedDynamicTraceStr = `
binary_spec {
  upid {
    asid: 306070887 pid: 3902477011 ts_ns: 11841725277501915136
  }
  language: GOLANG
}
outputs {
  name: "http_return_table"
  fields: "id"
  fields: "err"
  fields: "latency"
}
probes {
  name: "http_return0"
  trace_point {
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
    variable_name: "arg0"
    variable_name: "ret0"
    variable_name: "lat0"
	}
	ttl_ns: 300000000000
}
`

// TestPlanner_CompileRequest makes sure that we can actually pass in all the info needed
// to create a PlannerState and can successfully compile to an expected result.
func TestPlanner_CompileRequest(t *testing.T) {
	// Create the compiler.
	var udfInfoPb udfspb.UDFInfo
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	if !assert.Nil(t, err) {
		t.FailNow()
	}
	err = proto.Unmarshal(b, &udfInfoPb)
	if !assert.Nil(t, err) {
		t.FailNow()
	}
	c := logicalplanner.New(&udfInfoPb)
	defer c.Free()
	// Pass the relation proto, table and query to the compilation.
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	compileRequestPB := &plannerpb.CompileMutationsRequest{
		QueryStr: probePxl,
	}
	compileMutationResponse, err := c.CompileMutations(plannerStatePB, compileRequestPB)

	if err != nil {
		log.Fatalln("Failed to compile mutations:", err)
		os.Exit(1)
	}

	status := compileMutationResponse.Status
	if !assert.Equal(t, status.ErrCode, statuspb.OK) {
		var errorPB compilerpb.CompilerErrorGroup
		logicalplanner.GetCompilerErrorContext(status, &errorPB)
		log.Infof("%v", errorPB)
		log.Infof("%v", status)
	}

	var expectedDynamicTracePb logical.Program

	if err = proto.UnmarshalText(expectedDynamicTraceStr, &expectedDynamicTracePb); err != nil {
		log.Fatalf("Failed to unmarshal expected proto", err)
		t.FailNow()
	}

	assert.Equal(t, &expectedDynamicTracePb, compileMutationResponse.Trace)
}
