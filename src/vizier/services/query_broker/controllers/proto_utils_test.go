package controllers_test

import (
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	plannerpb "pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

var boolScalarValuePb = `
data_type: BOOLEAN
bool_value: false
`

var queryReqPb = `
query_str: "abcd this is a test"
exec_funcs {
	func_name: "f"
	arg_values {
		name: "a"
		value: "1"
	}
	output_table_prefix: "table1"
}
exec_funcs {
	func_name: "g"
	arg_values {
		name: "c"
		value: "3"
	}
	arg_values {
		name: "d"
		value: "4"
	}
	output_table_prefix: "table2"
}
`

var executeScriptReqPb = `
query_str: "abcd this is a test"
exec_funcs {
	func_name: "f"
	arg_values {
		name: "a"
		value: "1"
	}
	output_table_prefix: "table1"
}
exec_funcs {
	func_name: "g"
	arg_values {
		name: "c"
		value: "3"
	}
	arg_values {
		name: "d"
		value: "4"
	}
	output_table_prefix: "table2"
}
`

var tablePb = `
relation {
	columns {
		column_name: "abcd"
		column_type: BOOLEAN
		column_desc: "this is a boolean column"
	}
	columns {
		column_name: "efgh"
		column_type: INT64
		column_desc: "a test column in a test table"
	}
}
name: "test"
`

var tableSemanticTypePb = `
relation {
	columns {
		column_name: "abcd"
		column_type: STRING
		column_semantic_type: ST_SERVICE_NAME
	}
	columns {
		column_name: "efgh"
		column_type: STRING
		column_semantic_type: ST_POD_NAME
	}
}
name: "test"
`

var unauthenticatedStatusPb = `
err_code: UNAUTHENTICATED
msg: "this is a message"
`

var timingStatsPb = `
timing_info {
	execution_time_ns: 10
	compilation_time_ns: 4
}
execution_stats {
	timing {
		execution_time_ns: 10
		compilation_time_ns: 4
	}	
	bytes_processed: 100
	records_processed: 50
}
`

var vizierTimingStatsPb = `
execution_stats {
	timing {
		execution_time_ns: 10
		compilation_time_ns: 4
	}
	bytes_processed: 100
	records_processed: 50
}
`

var uint128Pb = `
	low: 123
	high: 456
`

var rowBatchPb = `
	eow: false
	eos: true
	num_rows: 10
	cols {
		boolean_data {
			data: true
			data: false
			data: true
		}
	}
	cols {
		string_data {
			data: "abcd"
			data: "efgh"
			data: "ijkl"
		}
	}
`

func TestVizierQueryRequestToPlannerQueryRequest(t *testing.T) {
	sv := new(vizierpb.ExecuteScriptRequest)
	if err := proto.UnmarshalText(executeScriptReqPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	expectedQr := new(plannerpb.QueryRequest)
	if err := proto.UnmarshalText(queryReqPb, expectedQr); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	qr, err := controllers.VizierQueryRequestToPlannerQueryRequest(sv)
	assert.Nil(t, err)
	assert.Equal(t, expectedQr, qr)
}

func TestStatusToVizierStatus(t *testing.T) {
	sv := new(statuspb.Status)
	if err := proto.UnmarshalText(unauthenticatedStatusPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	s := controllers.StatusToVizierStatus(sv)
	assert.Equal(t, "this is a message", s.Message)
	assert.Equal(t, int32(16), s.Code)
}

func TestCompilerErrorStatusToVizierStatus(t *testing.T) {
	errs := make([]*compilerpb.CompilerError, 2)
	errs[0] = &compilerpb.CompilerError{
		Error: &compilerpb.CompilerError_LineColError{
			LineColError: &compilerpb.LineColError{
				Line:    1,
				Column:  2,
				Message: "compilation error here",
			},
		},
	}
	errs[1] = &compilerpb.CompilerError{
		Error: &compilerpb.CompilerError_LineColError{
			LineColError: &compilerpb.LineColError{
				Line:    101,
				Column:  200,
				Message: "another compilation error here",
			},
		},
	}
	compilerEG := &compilerpb.CompilerErrorGroup{
		Errors: errs,
	}
	compilerEGAny, err := types.MarshalAny(compilerEG)
	assert.Nil(t, err)
	sv := &statuspb.Status{
		Context: compilerEGAny,
	}

	s := controllers.StatusToVizierStatus(sv)
	assert.Equal(t, 2, len(s.ErrorDetails))
	assert.Equal(t, uint64(1), s.ErrorDetails[0].GetCompilerError().Line)
	assert.Equal(t, uint64(2), s.ErrorDetails[0].GetCompilerError().Column)
	assert.Equal(t, "compilation error here", s.ErrorDetails[0].GetCompilerError().Message)
	assert.Equal(t, uint64(101), s.ErrorDetails[1].GetCompilerError().Line)
	assert.Equal(t, uint64(200), s.ErrorDetails[1].GetCompilerError().Column)
	assert.Equal(t, "another compilation error here", s.ErrorDetails[1].GetCompilerError().Message)
}

func TestRelationFromTable(t *testing.T) {
	sv := new(schemapb.Table)
	if err := proto.UnmarshalText(tablePb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	expectedQm := new(vizierpb.QueryMetadata)
	if err := proto.UnmarshalText(tablePb, expectedQm); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	qm, err := controllers.RelationFromTable(sv)
	assert.Nil(t, err)
	assert.Equal(t, expectedQm, qm)
}

func TestRelationFromTableWithSemanticTypes(t *testing.T) {
	sv := new(schemapb.Table)
	if err := proto.UnmarshalText(tableSemanticTypePb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	expectedQm := new(vizierpb.QueryMetadata)
	if err := proto.UnmarshalText(tableSemanticTypePb, expectedQm); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	qm, err := controllers.RelationFromTable(sv)
	assert.Nil(t, err)
	assert.Equal(t, expectedQm, qm)
}

func TestQueryResultStatsToVizierStats(t *testing.T) {
	sv := new(queryresultspb.QueryResult)
	if err := proto.UnmarshalText(timingStatsPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	expectedQd := new(vizierpb.QueryData)
	if err := proto.UnmarshalText(vizierTimingStatsPb, expectedQd); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	qm := controllers.QueryResultStatsToVizierStats(sv.ExecutionStats, 4)
	assert.Equal(t, expectedQd.ExecutionStats, qm)
}

func TestUInt128ToVizierUInt128(t *testing.T) {
	sv := new(typespb.UInt128)
	if err := proto.UnmarshalText(uint128Pb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	expectedQd := new(vizierpb.UInt128)
	if err := proto.UnmarshalText(uint128Pb, expectedQd); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	qm := controllers.UInt128ToVizierUInt128(sv)
	assert.Equal(t, expectedQd, qm)
}

func TestRowBatchToVizierRowBatch(t *testing.T) {
	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	expectedQd := new(vizierpb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, expectedQd); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	qm, err := controllers.RowBatchToVizierRowBatch(sv, "")
	assert.Nil(t, err)
	assert.Equal(t, expectedQd, qm)
}

func TestBuildExecuteScriptResponse(t *testing.T) {
	// TODO(nserrino): Fill this in.
}

func TestQueryPlanResponse(t *testing.T) {
	// TODO(nserrino): Fill this in.
}
