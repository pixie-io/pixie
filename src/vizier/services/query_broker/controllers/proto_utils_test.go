package controllers_test

import (
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"

	plannerpb "pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	planpb "pixielabs.ai/pixielabs/src/carnot/planpb"
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

var executeScriptReqPb = `
query_str: "abcd this is a test"
flag_values {
	flag_name: "flag1"
	flag_value {
		data_type: BOOLEAN
		bool_value: false		
	}
}
flag_values {
	flag_name: "intflag"
	flag_value {
		data_type: INT64
		int64_value: 1234		
	}
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

func TestVizierScalarValueToPlanScalarValue(t *testing.T) {
	sv := new(vizierpb.ScalarValue)
	if err := proto.UnmarshalText(boolScalarValuePb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	expectedSvPlan := new(planpb.ScalarValue)
	if err := proto.UnmarshalText(boolScalarValuePb, expectedSvPlan); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	svPlan, err := controllers.VizierScalarValueToPlanScalarValue(sv)
	assert.Nil(t, err)
	assert.Equal(t, expectedSvPlan, svPlan)
}

func TestVizierQueryRequestToPlannerQueryRequest(t *testing.T) {
	sv := new(vizierpb.ExecuteScriptRequest)
	if err := proto.UnmarshalText(executeScriptReqPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	expectedQr := new(plannerpb.QueryRequest)
	if err := proto.UnmarshalText(executeScriptReqPb, expectedQr); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	qr, err := controllers.VizierQueryRequestToPlannerQueryRequest(sv)
	assert.Nil(t, err)
	assert.Equal(t, expectedQr, qr)
}

func TestStatusToVizierStatus(t *testing.T) {
	sv := new(statuspb.Status)
	if err := proto.UnmarshalText(unauthenticatedStatusPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	s, err := controllers.StatusToVizierStatus(sv)
	assert.Nil(t, err)
	assert.Equal(t, "this is a message", s.Message)
	assert.Equal(t, int32(16), s.Code)
}

func TestRelationFromTable(t *testing.T) {
	sv := new(schemapb.Table)
	if err := proto.UnmarshalText(tablePb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	expectedQm := new(vizierpb.QueryMetadata)
	if err := proto.UnmarshalText(tablePb, expectedQm); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	qm, err := controllers.RelationFromTable(sv)
	assert.Nil(t, err)
	assert.Equal(t, expectedQm, qm)
}

func TestQueryResultStatsToVizierStats(t *testing.T) {
	sv := new(queryresultspb.QueryResult)
	if err := proto.UnmarshalText(timingStatsPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	expectedQd := new(vizierpb.QueryData)
	if err := proto.UnmarshalText(vizierTimingStatsPb, expectedQd); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	qm, err := controllers.QueryResultStatsToVizierStats(sv)
	assert.Nil(t, err)
	assert.Equal(t, expectedQd, qm)
}

func TestUInt128ToVizierUInt128(t *testing.T) {
	sv := new(typespb.UInt128)
	if err := proto.UnmarshalText(uint128Pb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	expectedQd := new(vizierpb.UInt128)
	if err := proto.UnmarshalText(uint128Pb, expectedQd); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	qm := controllers.UInt128ToVizierUInt128(sv)
	assert.Equal(t, expectedQd, qm)
}

func TestRowBatchToVizierRowBatch(t *testing.T) {
	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	expectedQd := new(vizierpb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, expectedQd); err != nil {
		t.Fatalf("Cannot unmarshal proto")
	}

	qm, err := controllers.RowBatchToVizierRowBatch(sv)
	assert.Nil(t, err)
	assert.Equal(t, expectedQd, qm)
}
