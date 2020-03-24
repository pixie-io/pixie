package logicalplanner_test

import (
	"log"
	"os"
	"testing"

	"pixielabs.ai/pixielabs/src/shared/scriptspb"

	"pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"

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
      }
      columns {
        column_name: "cpu_cycles"
        column_type: INT64
      }
      columns {
        column_name: "upid"
        column_type: UINT128
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
	query := "df = px.DataFrame(table='table1')\npx.display(df, 'out')"
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

	assert.Equal(t, agent1MemSrc1.Tablet, "1")
	assert.Equal(t, agent1MemSrc2.Tablet, "2")

	agent2Plan := planPB.QbAddressToPlan["agent2"]
	agent2MemSrc1 := agent2Plan.Nodes[0].Nodes[0].Op.GetMemSourceOp()
	agent2MemSrc2 := agent2Plan.Nodes[0].Nodes[1].Op.GetMemSourceOp()
	if !assert.NotNil(t, agent2MemSrc1) {
		t.FailNow()
	}
	if !assert.NotNil(t, agent2MemSrc2) {
		t.FailNow()
	}
	assert.Equal(t, agent2MemSrc1.Tablet, "3")
	assert.Equal(t, agent2MemSrc2.Tablet, "4")
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
	query := "df = px.DataFrame(table='bad_table')\npx.display(df, 'out')"
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

const mainFuncArgsQuery = `
def main(foo : str):
		queryDF = px.DataFrame(table='cpu', select=['cpu0'])
		queryDF['foo_flag'] = foo
		px.display(queryDF, 'map')
`
const mainFuncArgsPBStr = `
	args {
		data_type: STRING
		semantic_type: ST_NONE
		name: "foo"
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
	query := "px.display(px.DataFrame('http_events'))"
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

const vizFuncsQuery = `
import px
@px.viz.vega("vega spec for f")
def f(start_time: px.Time, end_time: px.Time, svc: str):
  """Doc string for f"""
  return 1

@px.viz.vega("vega spec for g")
def g(a: int, b: float):
  """Doc string for g"""
  return 1
`

const expectedVizFuncsInfoPBStr = `
doc_string_map {
  key: "f"
  value: "Doc string for f"
}
doc_string_map {
  key: "g"
  value: "Doc string for g"
}
viz_spec_map {
  key: "f"
  value {
    vega_spec: "vega spec for f"
  }
}
viz_spec_map {
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
			semantic_type: ST_NONE
      name: "start_time"
    }
    args {
      data_type: TIME64NS
			semantic_type: ST_NONE
      name: "end_time"
    }
    args {
      data_type: STRING
			semantic_type: ST_NONE
      name: "svc"
    }
  }
}
fn_args_map {
  key: "g"
  value {
    args {
      data_type: INT64
			semantic_type: ST_NONE
			name: "a"
    }
    args {
      data_type: FLOAT64
			semantic_type: ST_NONE
      name: "b"
    }
  }
}
`

func TestPlanner_ParseScriptForVizFuncsInfo(t *testing.T) {
	c := logicalplanner.New(&udfspb.UDFInfo{})
	defer c.Free()

	vizFuncsResult, err := c.ParseScriptForVizFuncsInfo(vizFuncsQuery)

	if err != nil {
		log.Fatalln("Failed to get viz funcs info: ", err)
		t.FailNow()
	}

	status := vizFuncsResult.Status
	assert.Equal(t, status.ErrCode, statuspb.OK)

	var expectedVizFuncsInfoPb scriptspb.VizFuncsInfo

	if err = proto.UnmarshalText(expectedVizFuncsInfoPBStr, &expectedVizFuncsInfoPb); err != nil {
		log.Fatalf("Failed to unmarshal expected proto", err)
		t.FailNow()
	}

	assert.Equal(t, &expectedVizFuncsInfoPb, vizFuncsResult.Info)
}
