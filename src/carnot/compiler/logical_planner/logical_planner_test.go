package logicalplanner_test

import (
	"log"
	"os"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/carnot/compiler/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/compiler/distributedpb"
	logicalplanner "pixielabs.ai/pixielabs/src/carnot/compiler/logical_planner"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
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
	c := logicalplanner.New()
	defer c.Free()
	// Pass the relation proto, table and query to the compilation.
	query := "dataframe(table='table1').result(name='out')"
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	plannerResultPB, err := c.Plan(plannerStatePB, query)

	if err != nil {
		log.Fatalln("Failed to plan:", err)
		os.Exit(1)
	}

	status := plannerResultPB.Status
	assert.Equal(t, status.ErrCode, statuspb.OK)

	planPB := plannerResultPB.Plan
	assert.Equal(t, 2, len(planPB.Dag.GetNodes()))
	agent1Plan := planPB.QbAddressToPlan["agent1"]
	agent1MemSrc1 := agent1Plan.Nodes[0].Nodes[0].Op.GetMemSourceOp()
	agent1MemSrc2 := agent1Plan.Nodes[0].Nodes[1].Op.GetMemSourceOp()
	if !assert.NotNil(t, agent1MemSrc1) {
		t.FailNow()
	}
	if !assert.NotNil(t, agent1MemSrc2) {
		t.FailNow()
	}

	assert.Equal(t, agent1MemSrc1.Tablet, "2")
	assert.Equal(t, agent1MemSrc2.Tablet, "1")

	agent2Plan := planPB.QbAddressToPlan["agent2"]
	agent2MemSrc1 := agent2Plan.Nodes[0].Nodes[0].Op.GetMemSourceOp()
	agent2MemSrc2 := agent2Plan.Nodes[0].Nodes[1].Op.GetMemSourceOp()
	if !assert.NotNil(t, agent2MemSrc1) {
		t.FailNow()
	}
	if !assert.NotNil(t, agent2MemSrc2) {
		t.FailNow()
	}
	assert.Equal(t, agent2MemSrc1.Tablet, "4")
	assert.Equal(t, agent2MemSrc2.Tablet, "3")
	// TODO(philkuz) (PL-873) uncomment these when Kelvin is enabled.
	// agent1GRPCSink := agent1Plan.Nodes[0].Nodes[len(agent1Plan.Nodes[0].Nodes)-1].Op.GetGrpcSinkOp()
	// if !assert.NotNil(t, agent1GrpcSink) {
	// 	t.FailNow()
	// }
	// assert.Equal(t, agent1GRPCSink.Address, "1111")
	// assert.Equal(t, agent1GRPCSink.DestinationId, "agent1:0")
	// agent2GRPCSink := agent2Plan.Nodes[0].Nodes[len(agent2Plan.Nodes[0].Nodes)-1].Op.GetGrpcSinkOp()
	// if !assert.NotNil(t, agent2GrpcSink) {
	// 	t.FailNow()
	// }
	// assert.Equal(t, agent2GRPCSink.Address, "1111")
	// assert.Equal(t, agent2GRPCSink.DestinationId, "agent2:0")
	// agent3Plan := planPB.QbAddressToPlan["kelvin"]
	// agent3GrpcSource1 := agent3Plan.Nodes[0].Nodes[0].Op.GetGrpcSourceOp()
	// agent3GrpcSource2 := agent3Plan.Nodes[0].Nodes[1].Op.GetGrpcSourceOp()
	// if !assert.NotNil(t, agent3GrpcSource1) {
	// 	t.FailNow()
	// }
	// if !assert.NotNil(t, agent3GrpcSource2) {
	// 	t.FailNow()
	// }
	// assert.Equal(t, agent3GrpcSource1.SourceId, "agent1:0")
	// assert.Equal(t, agent3GrpcSource2.SourceId, "agent2:0")
}

func TestPlanner_MissingTable(t *testing.T) {
	// Create the compiler.
	c := logicalplanner.New()
	defer c.Free()
	// Pass the relation proto, table and query to the compilation.
	query := "dataframe(table='bad_table').result(name='out')"
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	proto.UnmarshalText(plannerStatePBStr, plannerStatePB)
	plannerResultPB, err := c.Plan(plannerStatePB, query)

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
