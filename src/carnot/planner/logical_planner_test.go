package logicalplanner_test

import (
	"log"
	"os"
	"testing"

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

const flagsQuery = `
px.flags('foo', type=str, description='a random param', default='default')
px.flags.parse()
queryDF = px.DataFrame(table='cpu', select=['cpu0'])
queryDF['foo_flag'] = px.flags.foo
px.display(queryDF, 'map')
`

const expectedFlagsPBStr = `
	flags {
		data_type: STRING
		semantic_type: ST_NONE
		name: "foo"
		description: "a random param"
		default_value: {
			data_type: STRING
			string_value: "default"
		}
	}`

// TestPlanner_GetAvailableFlags makes sure that we can call GetAvailableFlags
// and we receive a status ok, as well as the correct flags.
func TestPlanner_GetAvailableFlags(t *testing.T) {
	// Create the planner.
	c := logicalplanner.New(&udfspb.UDFInfo{})
	defer c.Free()
	// Note that the string can't be empty since the cgo interface treats an empty string
	// as an error
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: flagsQuery,
	}
	getFlagsResultPB, err := c.GetAvailableFlags(queryRequestPB)

	if err != nil {
		log.Fatalln("Failed to get flags: ", err)
		t.FailNow()
	}

	status := getFlagsResultPB.Status
	assert.Equal(t, status.ErrCode, statuspb.OK)

	var expectedFlagsPB plannerpb.QueryFlagsSpec

	if err = proto.UnmarshalText(expectedFlagsPBStr, &expectedFlagsPB); err != nil {
		log.Fatalf("Failed to unmarshal expected proto", err)
		t.FailNow()
	}

	assert.Equal(t, &expectedFlagsPB, getFlagsResultPB.QueryFlags)
}

func TestPlanner_GetAvailableFlags_BadQuery(t *testing.T) {
	// Create the compiler.
	c := logicalplanner.New(&udfspb.UDFInfo{})
	defer c.Free()
	// query causes a syntax error.
	query := "px.flags("
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: query,
	}
	plannerResultPB, err := c.Plan(plannerStatePB, queryRequestPB)

	if err != nil {
		log.Fatalln("Failed to plan:", err)
		t.FailNow()
	}

	status := plannerResultPB.Status
	assert.NotEqual(t, status.ErrCode, statuspb.OK)
	var errorPB compilerpb.CompilerErrorGroup
	err = logicalplanner.GetCompilerErrorContext(status, &errorPB)

	if !assert.NoError(t, err) {
		t.FailNow()
	}

	// This error creates 3 errors.
	if !assert.Equal(t, 3, len(errorPB.Errors)) {
		t.FailNow()
	}
	compilerError := errorPB.Errors[0]
	lineColError := compilerError.GetLineColError()
	if !assert.NotNil(t, lineColError) {
		t.FailNow()
	}
	assert.Regexp(t, "SyntaxError: Expected `\\)`", lineColError.Message)

}
