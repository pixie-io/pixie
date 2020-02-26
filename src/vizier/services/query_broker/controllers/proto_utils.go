package controllers

import (
	"errors"

	gogotypes "github.com/gogo/protobuf/types"
	"google.golang.org/grpc/codes"

	plannerpb "pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	planpb "pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

var dataTypeToVizierDataType = map[typespb.DataType]vizierpb.DataType{
	typespb.DATA_TYPE_UNKNOWN: vizierpb.DATA_TYPE_UNKNOWN,
	typespb.BOOLEAN:           vizierpb.BOOLEAN,
	typespb.INT64:             vizierpb.INT64,
	typespb.UINT128:           vizierpb.UINT128,
	typespb.FLOAT64:           vizierpb.FLOAT64,
	typespb.STRING:            vizierpb.STRING,
	typespb.TIME64NS:          vizierpb.TIME64NS,
	typespb.DURATION64NS:      vizierpb.DURATION64NS,
}

// These codes are taken from https://godoc.org/google.golang.org/grpc/codes#Code.
var statusCodeToGRPCCode = map[statuspb.Code]codes.Code{
	statuspb.OK:                   codes.OK,
	statuspb.CANCELLED:            codes.Canceled,
	statuspb.UNKNOWN:              codes.Unknown,
	statuspb.INVALID_ARGUMENT:     codes.InvalidArgument,
	statuspb.DEADLINE_EXCEEDED:    codes.DeadlineExceeded,
	statuspb.NOT_FOUND:            codes.NotFound,
	statuspb.ALREADY_EXISTS:       codes.AlreadyExists,
	statuspb.PERMISSION_DENIED:    codes.PermissionDenied,
	statuspb.UNAUTHENTICATED:      codes.Unauthenticated,
	statuspb.INTERNAL:             codes.Internal,
	statuspb.RESOURCE_UNAVAILABLE: codes.Unavailable,
	statuspb.SYSTEM:               codes.Internal,
}

// VizierScalarValueToPlanScalarValue converts an externally-facing scalar value to an internal representation of ScalarValue.
func VizierScalarValueToPlanScalarValue(vpb *vizierpb.ScalarValue) (*planpb.ScalarValue, error) {
	// PL_CARNOT_UPDATE_FOR_NEW_TYPES
	switch d := vpb.DataType; d {
	case vizierpb.BOOLEAN:
		return &planpb.ScalarValue{
			DataType: typespb.BOOLEAN,
			Value: &planpb.ScalarValue_BoolValue{
				BoolValue: vpb.GetBoolValue(),
			},
		}, nil
	case vizierpb.INT64:
		return &planpb.ScalarValue{
			DataType: typespb.INT64,
			Value: &planpb.ScalarValue_Int64Value{
				Int64Value: vpb.GetInt64Value(),
			},
		}, nil
	case vizierpb.FLOAT64:
		return &planpb.ScalarValue{
			DataType: typespb.FLOAT64,
			Value: &planpb.ScalarValue_Float64Value{
				Float64Value: vpb.GetFloat64Value(),
			},
		}, nil
	case vizierpb.STRING:
		return &planpb.ScalarValue{
			DataType: typespb.STRING,
			Value: &planpb.ScalarValue_StringValue{
				StringValue: vpb.GetStringValue(),
			},
		}, nil
	case vizierpb.TIME64NS:
		return &planpb.ScalarValue{
			DataType: typespb.TIME64NS,
			Value: &planpb.ScalarValue_Time64NsValue{
				Time64NsValue: vpb.GetTime64NsValue(),
			},
		}, nil
	case vizierpb.DURATION64NS:
		return &planpb.ScalarValue{
			DataType: typespb.DURATION64NS,
			Value: &planpb.ScalarValue_Duration64NsValue{
				Duration64NsValue: vpb.GetDuration64NsValue(),
			},
		}, nil
	default:
		return nil, errors.New("Could not convert Vizierpb ScalarValue to Planpb ScalarValue")
	}
}

// VizierQueryRequestToPlannerQueryRequest converts a externally-facing query request to an internal representation.
func VizierQueryRequestToPlannerQueryRequest(vpb *vizierpb.ExecuteScriptRequest) (*plannerpb.QueryRequest, error) {
	flags := make([]*plannerpb.QueryRequest_FlagValue, len(vpb.FlagValues))
	for i, f := range vpb.FlagValues {
		cFlag, err := VizierScalarValueToPlanScalarValue(f.FlagValue)
		if err != nil {
			return nil, err
		}
		flags[i] = &plannerpb.QueryRequest_FlagValue{
			FlagName:  f.FlagName,
			FlagValue: cFlag,
		}
	}
	return &plannerpb.QueryRequest{
		QueryStr:   vpb.QueryStr,
		FlagValues: flags,
	}, nil
}

// StatusToVizierStatus converts an internal status to an externally-facing Vizier status.
func StatusToVizierStatus(s *statuspb.Status) (*vizierpb.Status, error) {
	return &vizierpb.Status{
		Code:    int32(statusCodeToGRPCCode[s.ErrCode]),
		Message: s.Msg,
		Details: []*gogotypes.Any{s.Context},
	}, nil
}

// RelationFromTable gets the relation from the table.
func RelationFromTable(table *schemapb.Table) (*vizierpb.QueryMetadata, error) {
	cols := make([]*vizierpb.Relation_ColumnInfo, len(table.Relation.Columns))
	for i, c := range table.Relation.Columns {
		newCol := &vizierpb.Relation_ColumnInfo{
			ColumnName: c.ColumnName,
			ColumnDesc: c.ColumnDesc,
			ColumnType: dataTypeToVizierDataType[c.ColumnType],
		}
		cols[i] = newCol
	}

	return &vizierpb.QueryMetadata{
		Relation: &vizierpb.Relation{
			Columns: cols,
		},
		Name: table.Name,
	}, nil
}

// QueryResultStatsToVizierStats gets the execution stats from the query results.
func QueryResultStatsToVizierStats(s *queryresultspb.QueryResult) (*vizierpb.QueryData, error) {
	return &vizierpb.QueryData{
		ExecutionStats: &vizierpb.QueryExecutionStats{
			Timing: &vizierpb.QueryTimingInfo{
				ExecutionTimeNs:   s.TimingInfo.ExecutionTimeNs,
				CompilationTimeNs: s.TimingInfo.CompilationTimeNs,
			},
			BytesProcessed:   s.ExecutionStats.BytesProcessed,
			RecordsProcessed: s.ExecutionStats.RecordsProcessed,
		},
	}, nil
}

// UInt128ToVizierUInt128 converts our internal representation of UInt128 to Vizier's representation of UInt128.
func UInt128ToVizierUInt128(i *typespb.UInt128) *vizierpb.UInt128 {
	return &vizierpb.UInt128{
		Low:  i.Low,
		High: i.High,
	}
}

func colToVizierCol(col *schemapb.Column) (*vizierpb.Column, error) {
	switch c := col.ColData.(type) {
	case *schemapb.Column_BooleanData:
		return &vizierpb.Column{
			ColData: &vizierpb.Column_BooleanData{
				BooleanData: &vizierpb.BooleanColumn{
					Data: c.BooleanData.Data,
				},
			},
		}, nil
	case *schemapb.Column_Int64Data:
		return &vizierpb.Column{
			ColData: &vizierpb.Column_Int64Data{
				Int64Data: &vizierpb.Int64Column{
					Data: c.Int64Data.Data,
				},
			},
		}, nil
	case *schemapb.Column_Uint128Data:
		b := make([]*vizierpb.UInt128, len(c.Uint128Data.Data))
		for i, s := range c.Uint128Data.Data {
			b[i] = UInt128ToVizierUInt128(s)
		}

		return &vizierpb.Column{
			ColData: &vizierpb.Column_Uint128Data{
				Uint128Data: &vizierpb.UInt128Column{
					Data: b,
				},
			},
		}, nil
	case *schemapb.Column_Time64NsData:
		return &vizierpb.Column{
			ColData: &vizierpb.Column_Time64NsData{
				Time64NsData: &vizierpb.Time64NSColumn{
					Data: c.Time64NsData.Data,
				},
			},
		}, nil
	case *schemapb.Column_Float64Data:
		return &vizierpb.Column{
			ColData: &vizierpb.Column_Float64Data{
				Float64Data: &vizierpb.Float64Column{
					Data: c.Float64Data.Data,
				},
			},
		}, nil
	case *schemapb.Column_StringData:
		b := make([]string, len(c.StringData.Data))
		for i, s := range c.StringData.Data {
			b[i] = string(s)
		}
		return &vizierpb.Column{
			ColData: &vizierpb.Column_StringData{
				StringData: &vizierpb.StringColumn{
					Data: b,
				},
			},
		}, nil
	case *schemapb.Column_Duration64NsData:
		return &vizierpb.Column{
			ColData: &vizierpb.Column_Duration64NsData{
				Duration64NsData: &vizierpb.Duration64NSColumn{
					Data: c.Duration64NsData.Data,
				},
			},
		}, nil
	default:
		return nil, errors.New("Could not get column type")
	}
}

// RowBatchToVizierRowBatch converts an internal row batch to a vizier row batch.
func RowBatchToVizierRowBatch(rb *schemapb.RowBatchData) (*vizierpb.RowBatchData, error) {
	cols := make([]*vizierpb.Column, len(rb.Cols))
	for i, col := range rb.Cols {
		c, err := colToVizierCol(col)
		if err != nil {
			return nil, err
		}
		cols[i] = c
	}

	return &vizierpb.RowBatchData{
		NumRows: rb.NumRows,
		Eow:     rb.Eow,
		Eos:     rb.Eos,
		Cols:    cols,
	}, nil
}
