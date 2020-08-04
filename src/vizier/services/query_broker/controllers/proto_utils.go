package controllers

import (
	"errors"

	gogotypes "github.com/gogo/protobuf/types"
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	plannerpb "pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
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
}

var semanticTypeToVizierSemanticType = map[typespb.SemanticType]vizierpb.SemanticType{
	typespb.ST_UNSPECIFIED:      vizierpb.ST_UNSPECIFIED,
	typespb.ST_NONE:             vizierpb.ST_NONE,
	typespb.ST_AGENT_UID:        vizierpb.ST_AGENT_UID,
	typespb.ST_UPID:             vizierpb.ST_UPID,
	typespb.ST_SERVICE_NAME:     vizierpb.ST_SERVICE_NAME,
	typespb.ST_POD_NAME:         vizierpb.ST_POD_NAME,
	typespb.ST_POD_PHASE:        vizierpb.ST_POD_PHASE,
	typespb.ST_POD_STATUS:       vizierpb.ST_POD_STATUS,
	typespb.ST_NODE_NAME:        vizierpb.ST_NODE_NAME,
	typespb.ST_CONTAINER_NAME:   vizierpb.ST_CONTAINER_NAME,
	typespb.ST_CONTAINER_STATE:  vizierpb.ST_CONTAINER_STATE,
	typespb.ST_CONTAINER_STATUS: vizierpb.ST_CONTAINER_STATUS,
	typespb.ST_NAMESPACE_NAME:   vizierpb.ST_NAMESPACE_NAME,
	typespb.ST_BYTES:            vizierpb.ST_BYTES,
	typespb.ST_PERCENT:          vizierpb.ST_PERCENT,
	typespb.ST_DURATION_NS:      vizierpb.ST_DURATION_NS,
	typespb.ST_QUANTILES:        vizierpb.ST_QUANTILES,
	typespb.ST_IP_ADDRESS:       vizierpb.ST_IP_ADDRESS,
	typespb.ST_PORT:             vizierpb.ST_PORT,
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

var lifeCycleStateToVizierLifeCycleStateMap = map[statuspb.LifeCycleState]vizierpb.LifeCycleState{
	statuspb.UNKNOWN_STATE:    vizierpb.UNKNOWN_STATE,
	statuspb.PENDING_STATE:    vizierpb.PENDING_STATE,
	statuspb.RUNNING_STATE:    vizierpb.RUNNING_STATE,
	statuspb.TERMINATED_STATE: vizierpb.TERMINATED_STATE,
	statuspb.FAILED_STATE:     vizierpb.FAILED_STATE,
}

func convertLifeCycleStateToVizierLifeCycleState(state statuspb.LifeCycleState) vizierpb.LifeCycleState {
	if val, ok := lifeCycleStateToVizierLifeCycleStateMap[state]; ok {
		return val
	}
	return vizierpb.UNKNOWN_STATE
}

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

// VizierQueryRequestToPlannerMutationRequest maps request to mutation.
func VizierQueryRequestToPlannerMutationRequest(vpb *vizierpb.ExecuteScriptRequest) (*plannerpb.CompileMutationsRequest, error) {
	return &plannerpb.CompileMutationsRequest{
		QueryStr:  vpb.QueryStr,
		ExecFuncs: convertExecFuncs(vpb.ExecFuncs),
	}, nil
}

// VizierQueryRequestToPlannerQueryRequest converts a externally-facing query request to an internal representation.
func VizierQueryRequestToPlannerQueryRequest(vpb *vizierpb.ExecuteScriptRequest) (*plannerpb.QueryRequest, error) {
	return &plannerpb.QueryRequest{
		QueryStr:  vpb.QueryStr,
		ExecFuncs: convertExecFuncs(vpb.ExecFuncs),
	}, nil
}

// ErrToVizierResponse converts an error to an externally-facing Vizier response message
func ErrToVizierResponse(id uuid.UUID, err error) *vizierpb.ExecuteScriptResponse {
	return &vizierpb.ExecuteScriptResponse{
		QueryID: id.String(),
		Status:  ErrToVizierStatus(err),
	}
}

// ErrToVizierStatus converts an error to an externally-facing Vizier status.
func ErrToVizierStatus(err error) *vizierpb.Status {
	s, ok := status.FromError(err)
	if ok {
		return &vizierpb.Status{
			Code:    int32(s.Code()),
			Message: s.Message(),
		}
	}

	return &vizierpb.Status{
		Code:    int32(codes.Unknown),
		Message: err.Error(),
	}
}

// StatusToVizierResponse converts an error to an externally-facing Vizier response message
func StatusToVizierResponse(id uuid.UUID, s *statuspb.Status) *vizierpb.ExecuteScriptResponse {
	return &vizierpb.ExecuteScriptResponse{
		QueryID: id.String(),
		Status:  StatusToVizierStatus(s),
	}
}

// StatusToVizierStatus converts an internal status to an externally-facing Vizier status.
func StatusToVizierStatus(s *statuspb.Status) *vizierpb.Status {
	return &vizierpb.Status{
		Code:         int32(statusCodeToGRPCCode[s.ErrCode]),
		Message:      s.Msg,
		ErrorDetails: getErrorsFromStatusContext(s.Context),
	}
}

func getErrorsFromStatusContext(ctx *gogotypes.Any) []*vizierpb.ErrorDetails {
	errorPB := &compilerpb.CompilerErrorGroup{}
	if !gogotypes.Is(ctx, errorPB) {
		return nil
	}
	err := gogotypes.UnmarshalAny(ctx, errorPB)
	if err != nil {
		return nil
	}

	errors := make([]*vizierpb.ErrorDetails, len(errorPB.Errors))
	for i, e := range errorPB.Errors {
		lcErr := e.GetLineColError()
		errors[i] = &vizierpb.ErrorDetails{
			Error: &vizierpb.ErrorDetails_CompilerError{
				CompilerError: &vizierpb.CompilerError{
					Line:    lcErr.Line,
					Column:  lcErr.Column,
					Message: lcErr.Message,
				},
			},
		}
	}
	return errors
}

// RelationFromTable gets the relation from the table.
func RelationFromTable(table *schemapb.Table) (*vizierpb.QueryMetadata, error) {
	cols := make([]*vizierpb.Relation_ColumnInfo, len(table.Relation.Columns))
	for i, c := range table.Relation.Columns {
		newCol := &vizierpb.Relation_ColumnInfo{
			ColumnName:         c.ColumnName,
			ColumnDesc:         c.ColumnDesc,
			ColumnType:         dataTypeToVizierDataType[c.ColumnType],
			ColumnSemanticType: semanticTypeToVizierSemanticType[c.ColumnSemanticType],
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
