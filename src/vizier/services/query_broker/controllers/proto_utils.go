package controllers

import (
	"errors"
	"fmt"

	gogotypes "github.com/gogo/protobuf/types"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	plannerpb "pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/carnotpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/utils"
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
func QueryResultStatsToVizierStats(e *queryresultspb.QueryExecutionStats, compilationTimeNs int64) *vizierpb.QueryExecutionStats {
	return &vizierpb.QueryExecutionStats{
		Timing: &vizierpb.QueryTimingInfo{
			ExecutionTimeNs:   e.Timing.ExecutionTimeNs,
			CompilationTimeNs: compilationTimeNs,
		},
		BytesProcessed:   e.BytesProcessed,
		RecordsProcessed: e.RecordsProcessed,
	}
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
func RowBatchToVizierRowBatch(rb *schemapb.RowBatchData, tableID string) (*vizierpb.RowBatchData, error) {
	cols := make([]*vizierpb.Column, len(rb.Cols))
	for i, col := range rb.Cols {
		c, err := colToVizierCol(col)
		if err != nil {
			return nil, err
		}
		cols[i] = c
	}

	return &vizierpb.RowBatchData{
		TableID: tableID,
		NumRows: rb.NumRows,
		Eow:     rb.Eow,
		Eos:     rb.Eos,
		Cols:    cols,
	}, nil
}

// BuildExecuteScriptResponse Converts the agent-format result into the vizier client format result.
func BuildExecuteScriptResponse(r *carnotpb.TransferResultChunkRequest,
	// Map of the received table names to their table ID on the output proto.
	tableIDMap map[string]string,
	compilationTimeNs int64) (*vizierpb.ExecuteScriptResponse, error) {

	res := &vizierpb.ExecuteScriptResponse{
		QueryID: utils.UUIDFromProtoOrNil(r.QueryID).String(),
	}

	if execStats := r.GetExecutionAndTimingInfo(); execStats != nil {
		stats := QueryResultStatsToVizierStats(execStats.ExecutionStats, compilationTimeNs)
		res.Result = &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				ExecutionStats: stats,
			},
		}
	}
	if queryResult := r.GetQueryResult(); queryResult != nil {
		// This agent message type will not turn into a message on the client stream.
		if queryResult.GetInitiateResultStream() {
			return nil, nil
		}

		tableName := queryResult.GetTableName()
		tableID, present := tableIDMap[tableName]
		if !present {
			return nil, fmt.Errorf("table %s does not have an ID in the table ID map", tableName)
		}
		if queryResult.GetRowBatch() == nil {
			return nil, fmt.Errorf("BuildExecuteScriptResponse expected a non-nil row batch")
		}

		batch, err := RowBatchToVizierRowBatch(queryResult.GetRowBatch(), tableID)
		if err != nil {
			return nil, err
		}
		res.Result = &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: batch,
			},
		}
	}
	if res.Result == nil {
		return nil, fmt.Errorf("error in ForwardQueryResult: Expected TransferResultChunkRequest to have row batch or exec stats")
	}
	return res, nil
}

// QueryPlanResponse returns the query plan as an ExecuteScriptResponse.
func QueryPlanResponse(queryID uuid.UUID, plan *distributedpb.DistributedPlan, planMap map[uuid.UUID]*planpb.Plan,
	agentStats *[]*queryresultspb.AgentExecutionStats,
	planTableID string) (*vizierpb.ExecuteScriptResponse, error) {

	queryPlan, err := getQueryPlanAsDotString(plan, planMap, agentStats)
	if err != nil {
		log.WithError(err).Error("error with query plan")
		return nil, err
	}

	batch := &vizierpb.RowBatchData{
		TableID: planTableID,
		Cols: []*vizierpb.Column{
			&vizierpb.Column{
				ColData: &vizierpb.Column_StringData{
					StringData: &vizierpb.StringColumn{
						Data: []string{queryPlan},
					},
				},
			},
		},
		NumRows: 1,
		Eos:     true,
		Eow:     true,
	}

	return &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: batch,
			},
		},
	}, nil
}

// QueryPlanRelationResponse returns the relation of the query plan as an ExecuteScriptResponse.
func QueryPlanRelationResponse(queryID uuid.UUID, planTableID string) *vizierpb.ExecuteScriptResponse {
	return &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_MetaData{
			MetaData: &vizierpb.QueryMetadata{
				Name: "__query_plan__",
				ID:   planTableID,
				Relation: &vizierpb.Relation{
					Columns: []*vizierpb.Relation_ColumnInfo{
						{
							ColumnName: "query_plan",
							ColumnType: vizierpb.STRING,
							ColumnDesc: "The query plan",
						},
					},
				},
			},
		},
	}
}

// OutputSchemaFromPlan takes in a plan map and returns the relations for all of the final output
// tables in the plan map.
func OutputSchemaFromPlan(planMap map[uuid.UUID]*planpb.Plan) map[string]*schemapb.Relation {
	outputRelations := make(map[string]*schemapb.Relation)

	for _, plan := range planMap {
		for _, fragment := range plan.Nodes {
			for _, node := range fragment.Nodes {
				if node.Op.OpType == planpb.GRPC_SINK_OPERATOR {
					grpcSink := node.Op.GetGRPCSinkOp()
					outputTableInfo := grpcSink.GetOutputTable()
					if outputTableInfo == nil {
						continue
					}
					relation := &schemapb.Relation{
						Columns: []*schemapb.Relation_ColumnInfo{},
					}
					for i, colName := range outputTableInfo.ColumnNames {
						relation.Columns = append(relation.Columns, &schemapb.Relation_ColumnInfo{
							ColumnName:         colName,
							ColumnType:         outputTableInfo.ColumnTypes[i],
							ColumnSemanticType: outputTableInfo.ColumnSemanticTypes[i],
						})
					}
					outputRelations[outputTableInfo.TableName] = relation
				}
			}
		}
	}
	return outputRelations
}

// AgentRelationToVizierRelation converts the agent relation format to the Vizier relation format.
func AgentRelationToVizierRelation(relation *schemapb.Relation) *vizierpb.Relation {
	var cols []*vizierpb.Relation_ColumnInfo

	for _, c := range relation.Columns {
		newCol := &vizierpb.Relation_ColumnInfo{
			ColumnName:         c.ColumnName,
			ColumnDesc:         c.ColumnDesc,
			ColumnType:         dataTypeToVizierDataType[c.ColumnType],
			ColumnSemanticType: semanticTypeToVizierSemanticType[c.ColumnSemanticType],
		}
		cols = append(cols, newCol)
	}

	return &vizierpb.Relation{
		Columns: cols,
	}
}

// TableRelationResponses returns the query metadata table schemas as ExecuteScriptResponses.
func TableRelationResponses(queryID uuid.UUID, tableIDMap map[string]string,
	planMap map[uuid.UUID]*planpb.Plan) ([]*vizierpb.ExecuteScriptResponse, error) {

	var results []*vizierpb.ExecuteScriptResponse
	schemas := OutputSchemaFromPlan(planMap)

	for tableName, schema := range schemas {
		tableID, present := tableIDMap[tableName]
		if !present {
			return nil, fmt.Errorf("Table ID for table name %s not found in table map", tableName)
		}

		convertedRelation := AgentRelationToVizierRelation(schema)
		results = append(results, &vizierpb.ExecuteScriptResponse{
			QueryID: queryID.String(),
			Result: &vizierpb.ExecuteScriptResponse_MetaData{
				MetaData: &vizierpb.QueryMetadata{
					Name:     tableName,
					ID:       tableID,
					Relation: convertedRelation,
				},
			},
		})
	}

	return results, nil
}
