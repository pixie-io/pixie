package controllers

import (
	"errors"
	"fmt"

	"github.com/gofrs/uuid"
	gogotypes "github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	public_vizierapipb "px.dev/pixie/src/api/public/vizierapipb"
	"px.dev/pixie/src/carnot/carnotpb"
	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/carnot/queryresultspb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/shared/types/typespb"
	"px.dev/pixie/src/table_store/schemapb"
	"px.dev/pixie/src/utils"
)

var dataTypeToVizierDataType = map[typespb.DataType]public_vizierapipb.DataType{
	typespb.DATA_TYPE_UNKNOWN: public_vizierapipb.DATA_TYPE_UNKNOWN,
	typespb.BOOLEAN:           public_vizierapipb.BOOLEAN,
	typespb.INT64:             public_vizierapipb.INT64,
	typespb.UINT128:           public_vizierapipb.UINT128,
	typespb.FLOAT64:           public_vizierapipb.FLOAT64,
	typespb.STRING:            public_vizierapipb.STRING,
	typespb.TIME64NS:          public_vizierapipb.TIME64NS,
}

var semanticTypeToVizierSemanticType = map[typespb.SemanticType]public_vizierapipb.SemanticType{
	typespb.ST_UNSPECIFIED:             public_vizierapipb.ST_UNSPECIFIED,
	typespb.ST_NONE:                    public_vizierapipb.ST_NONE,
	typespb.ST_TIME_NS:                 public_vizierapipb.ST_TIME_NS,
	typespb.ST_AGENT_UID:               public_vizierapipb.ST_AGENT_UID,
	typespb.ST_ASID:                    public_vizierapipb.ST_ASID,
	typespb.ST_UPID:                    public_vizierapipb.ST_UPID,
	typespb.ST_SERVICE_NAME:            public_vizierapipb.ST_SERVICE_NAME,
	typespb.ST_POD_NAME:                public_vizierapipb.ST_POD_NAME,
	typespb.ST_POD_PHASE:               public_vizierapipb.ST_POD_PHASE,
	typespb.ST_POD_STATUS:              public_vizierapipb.ST_POD_STATUS,
	typespb.ST_NODE_NAME:               public_vizierapipb.ST_NODE_NAME,
	typespb.ST_CONTAINER_NAME:          public_vizierapipb.ST_CONTAINER_NAME,
	typespb.ST_CONTAINER_STATE:         public_vizierapipb.ST_CONTAINER_STATE,
	typespb.ST_CONTAINER_STATUS:        public_vizierapipb.ST_CONTAINER_STATUS,
	typespb.ST_NAMESPACE_NAME:          public_vizierapipb.ST_NAMESPACE_NAME,
	typespb.ST_BYTES:                   public_vizierapipb.ST_BYTES,
	typespb.ST_PERCENT:                 public_vizierapipb.ST_PERCENT,
	typespb.ST_DURATION_NS:             public_vizierapipb.ST_DURATION_NS,
	typespb.ST_THROUGHPUT_PER_NS:       public_vizierapipb.ST_THROUGHPUT_PER_NS,
	typespb.ST_THROUGHPUT_BYTES_PER_NS: public_vizierapipb.ST_THROUGHPUT_BYTES_PER_NS,
	typespb.ST_QUANTILES:               public_vizierapipb.ST_QUANTILES,
	typespb.ST_DURATION_NS_QUANTILES:   public_vizierapipb.ST_DURATION_NS_QUANTILES,
	typespb.ST_IP_ADDRESS:              public_vizierapipb.ST_IP_ADDRESS,
	typespb.ST_PORT:                    public_vizierapipb.ST_PORT,
	typespb.ST_HTTP_REQ_METHOD:         public_vizierapipb.ST_HTTP_REQ_METHOD,
	typespb.ST_HTTP_RESP_STATUS:        public_vizierapipb.ST_HTTP_RESP_STATUS,
	typespb.ST_HTTP_RESP_MESSAGE:       public_vizierapipb.ST_HTTP_RESP_MESSAGE,
	typespb.ST_SCRIPT_REFERENCE:        public_vizierapipb.ST_SCRIPT_REFERENCE,
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

var lifeCycleStateToVizierLifeCycleStateMap = map[statuspb.LifeCycleState]public_vizierapipb.LifeCycleState{
	statuspb.UNKNOWN_STATE:    public_vizierapipb.UNKNOWN_STATE,
	statuspb.PENDING_STATE:    public_vizierapipb.PENDING_STATE,
	statuspb.RUNNING_STATE:    public_vizierapipb.RUNNING_STATE,
	statuspb.TERMINATED_STATE: public_vizierapipb.TERMINATED_STATE,
	statuspb.FAILED_STATE:     public_vizierapipb.FAILED_STATE,
}

func convertLifeCycleStateToVizierLifeCycleState(state statuspb.LifeCycleState) public_vizierapipb.LifeCycleState {
	if val, ok := lifeCycleStateToVizierLifeCycleStateMap[state]; ok {
		return val
	}
	return public_vizierapipb.UNKNOWN_STATE
}

func convertExecFuncs(inputFuncs []*public_vizierapipb.ExecuteScriptRequest_FuncToExecute) []*plannerpb.FuncToExecute {
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
func VizierQueryRequestToPlannerMutationRequest(vpb *public_vizierapipb.ExecuteScriptRequest) (*plannerpb.CompileMutationsRequest, error) {
	return &plannerpb.CompileMutationsRequest{
		QueryStr:  vpb.QueryStr,
		ExecFuncs: convertExecFuncs(vpb.ExecFuncs),
	}, nil
}

// VizierQueryRequestToPlannerQueryRequest converts a externally-facing query request to an internal representation.
func VizierQueryRequestToPlannerQueryRequest(vpb *public_vizierapipb.ExecuteScriptRequest) (*plannerpb.QueryRequest, error) {
	return &plannerpb.QueryRequest{
		QueryStr:  vpb.QueryStr,
		ExecFuncs: convertExecFuncs(vpb.ExecFuncs),
	}, nil
}

// ErrToVizierResponse converts an error to an externally-facing Vizier response message
func ErrToVizierResponse(id uuid.UUID, err error) *public_vizierapipb.ExecuteScriptResponse {
	return &public_vizierapipb.ExecuteScriptResponse{
		QueryID: id.String(),
		Status:  ErrToVizierStatus(err),
	}
}

// ErrToVizierStatus converts an error to an externally-facing Vizier status.
func ErrToVizierStatus(err error) *public_vizierapipb.Status {
	s, ok := status.FromError(err)
	if ok {
		return &public_vizierapipb.Status{
			Code:    int32(s.Code()),
			Message: s.Message(),
		}
	}

	return &public_vizierapipb.Status{
		Code:    int32(codes.Unknown),
		Message: err.Error(),
	}
}

// StatusToVizierResponse converts an error to an externally-facing Vizier response message
func StatusToVizierResponse(id uuid.UUID, s *statuspb.Status) *public_vizierapipb.ExecuteScriptResponse {
	return &public_vizierapipb.ExecuteScriptResponse{
		QueryID: id.String(),
		Status:  StatusToVizierStatus(s),
	}
}

// StatusToVizierStatus converts an internal status to an externally-facing Vizier status.
func StatusToVizierStatus(s *statuspb.Status) *public_vizierapipb.Status {
	return &public_vizierapipb.Status{
		Code:         int32(statusCodeToGRPCCode[s.ErrCode]),
		Message:      s.Msg,
		ErrorDetails: getErrorsFromStatusContext(s.Context),
	}
}

func getErrorsFromStatusContext(ctx *gogotypes.Any) []*public_vizierapipb.ErrorDetails {
	errorPB := &compilerpb.CompilerErrorGroup{}
	if !gogotypes.Is(ctx, errorPB) {
		return nil
	}
	err := gogotypes.UnmarshalAny(ctx, errorPB)
	if err != nil {
		return nil
	}

	errors := make([]*public_vizierapipb.ErrorDetails, len(errorPB.Errors))
	for i, e := range errorPB.Errors {
		lcErr := e.GetLineColError()
		errors[i] = &public_vizierapipb.ErrorDetails{
			Error: &public_vizierapipb.ErrorDetails_CompilerError{
				CompilerError: &public_vizierapipb.CompilerError{
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
func RelationFromTable(table *schemapb.Table) (*public_vizierapipb.QueryMetadata, error) {
	cols := make([]*public_vizierapipb.Relation_ColumnInfo, len(table.Relation.Columns))
	for i, c := range table.Relation.Columns {
		newCol := &public_vizierapipb.Relation_ColumnInfo{
			ColumnName:         c.ColumnName,
			ColumnDesc:         c.ColumnDesc,
			ColumnType:         dataTypeToVizierDataType[c.ColumnType],
			ColumnSemanticType: semanticTypeToVizierSemanticType[c.ColumnSemanticType],
		}
		cols[i] = newCol
	}

	return &public_vizierapipb.QueryMetadata{
		Relation: &public_vizierapipb.Relation{
			Columns: cols,
		},
		Name: table.Name,
	}, nil
}

// QueryResultStatsToVizierStats gets the execution stats from the query results.
func QueryResultStatsToVizierStats(e *queryresultspb.QueryExecutionStats, compilationTimeNs int64) *public_vizierapipb.QueryExecutionStats {
	return &public_vizierapipb.QueryExecutionStats{
		Timing: &public_vizierapipb.QueryTimingInfo{
			ExecutionTimeNs:   e.Timing.ExecutionTimeNs,
			CompilationTimeNs: compilationTimeNs,
		},
		BytesProcessed:   e.BytesProcessed,
		RecordsProcessed: e.RecordsProcessed,
	}
}

// UInt128ToVizierUInt128 converts our internal representation of UInt128 to Vizier's representation of UInt128.
func UInt128ToVizierUInt128(i *typespb.UInt128) *public_vizierapipb.UInt128 {
	return &public_vizierapipb.UInt128{
		Low:  i.Low,
		High: i.High,
	}
}

func colToVizierCol(col *schemapb.Column) (*public_vizierapipb.Column, error) {
	switch c := col.ColData.(type) {
	case *schemapb.Column_BooleanData:
		return &public_vizierapipb.Column{
			ColData: &public_vizierapipb.Column_BooleanData{
				BooleanData: &public_vizierapipb.BooleanColumn{
					Data: c.BooleanData.Data,
				},
			},
		}, nil
	case *schemapb.Column_Int64Data:
		return &public_vizierapipb.Column{
			ColData: &public_vizierapipb.Column_Int64Data{
				Int64Data: &public_vizierapipb.Int64Column{
					Data: c.Int64Data.Data,
				},
			},
		}, nil
	case *schemapb.Column_Uint128Data:
		b := make([]*public_vizierapipb.UInt128, len(c.Uint128Data.Data))
		for i, s := range c.Uint128Data.Data {
			b[i] = UInt128ToVizierUInt128(s)
		}

		return &public_vizierapipb.Column{
			ColData: &public_vizierapipb.Column_Uint128Data{
				Uint128Data: &public_vizierapipb.UInt128Column{
					Data: b,
				},
			},
		}, nil
	case *schemapb.Column_Time64NsData:
		return &public_vizierapipb.Column{
			ColData: &public_vizierapipb.Column_Time64NsData{
				Time64NsData: &public_vizierapipb.Time64NSColumn{
					Data: c.Time64NsData.Data,
				},
			},
		}, nil
	case *schemapb.Column_Float64Data:
		return &public_vizierapipb.Column{
			ColData: &public_vizierapipb.Column_Float64Data{
				Float64Data: &public_vizierapipb.Float64Column{
					Data: c.Float64Data.Data,
				},
			},
		}, nil
	case *schemapb.Column_StringData:
		b := make([]string, len(c.StringData.Data))
		for i, s := range c.StringData.Data {
			b[i] = string(s)
		}
		return &public_vizierapipb.Column{
			ColData: &public_vizierapipb.Column_StringData{
				StringData: &public_vizierapipb.StringColumn{
					Data: b,
				},
			},
		}, nil
	default:
		return nil, errors.New("Could not get column type")
	}
}

// RowBatchToVizierRowBatch converts an internal row batch to a vizier row batch.
func RowBatchToVizierRowBatch(rb *schemapb.RowBatchData, tableID string) (*public_vizierapipb.RowBatchData, error) {
	cols := make([]*public_vizierapipb.Column, len(rb.Cols))
	for i, col := range rb.Cols {
		c, err := colToVizierCol(col)
		if err != nil {
			return nil, err
		}
		cols[i] = c
	}

	return &public_vizierapipb.RowBatchData{
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
	compilationTimeNs int64) (*public_vizierapipb.ExecuteScriptResponse, error) {
	res := &public_vizierapipb.ExecuteScriptResponse{
		QueryID: utils.UUIDFromProtoOrNil(r.QueryID).String(),
	}

	if execStats := r.GetExecutionAndTimingInfo(); execStats != nil {
		stats := QueryResultStatsToVizierStats(execStats.ExecutionStats, compilationTimeNs)
		res.Result = &public_vizierapipb.ExecuteScriptResponse_Data{
			Data: &public_vizierapipb.QueryData{
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
		res.Result = &public_vizierapipb.ExecuteScriptResponse_Data{
			Data: &public_vizierapipb.QueryData{
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
	planTableID string,
	maxQueryPlanStringSizeBytes int) ([]*public_vizierapipb.ExecuteScriptResponse, error) {
	queryPlan, err := GetQueryPlanAsDotString(plan, planMap, agentStats)
	if err != nil {
		log.WithError(err).Error("error with query plan")
		return nil, err
	}

	var resp []*public_vizierapipb.ExecuteScriptResponse

	// We can't overwhelm NATS with a query plan greater than 1MB.
	for i := 0; i < len(queryPlan); i += maxQueryPlanStringSizeBytes {
		end := i + maxQueryPlanStringSizeBytes
		if end > len(queryPlan) {
			end = len(queryPlan)
		}

		last := end == len(queryPlan)
		batch := &public_vizierapipb.RowBatchData{
			TableID: planTableID,
			Cols: []*public_vizierapipb.Column{
				{
					ColData: &public_vizierapipb.Column_StringData{
						StringData: &public_vizierapipb.StringColumn{
							Data: []string{queryPlan[i:end]},
						},
					},
				},
			},
			NumRows: 1,
			Eos:     last,
			Eow:     last,
		}

		resp = append(resp, &public_vizierapipb.ExecuteScriptResponse{
			QueryID: queryID.String(),
			Result: &public_vizierapipb.ExecuteScriptResponse_Data{
				Data: &public_vizierapipb.QueryData{
					Batch: batch,
				},
			},
		})
	}

	return resp, nil
}

// QueryPlanRelationResponse returns the relation of the query plan as an ExecuteScriptResponse.
func QueryPlanRelationResponse(queryID uuid.UUID, planTableID string) *public_vizierapipb.ExecuteScriptResponse {
	return &public_vizierapipb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &public_vizierapipb.ExecuteScriptResponse_MetaData{
			MetaData: &public_vizierapipb.QueryMetadata{
				Name: "__query_plan__",
				ID:   planTableID,
				Relation: &public_vizierapipb.Relation{
					Columns: []*public_vizierapipb.Relation_ColumnInfo{
						{
							ColumnName: "query_plan",
							ColumnType: public_vizierapipb.STRING,
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
func AgentRelationToVizierRelation(relation *schemapb.Relation) *public_vizierapipb.Relation {
	var cols []*public_vizierapipb.Relation_ColumnInfo

	for _, c := range relation.Columns {
		newCol := &public_vizierapipb.Relation_ColumnInfo{
			ColumnName:         c.ColumnName,
			ColumnDesc:         c.ColumnDesc,
			ColumnType:         dataTypeToVizierDataType[c.ColumnType],
			ColumnSemanticType: semanticTypeToVizierSemanticType[c.ColumnSemanticType],
		}
		cols = append(cols, newCol)
	}

	return &public_vizierapipb.Relation{
		Columns: cols,
	}
}

// TableRelationResponses returns the query metadata table schemas as ExecuteScriptResponses.
func TableRelationResponses(queryID uuid.UUID, tableIDMap map[string]string,
	planMap map[uuid.UUID]*planpb.Plan) ([]*public_vizierapipb.ExecuteScriptResponse, error) {
	var results []*public_vizierapipb.ExecuteScriptResponse
	schemas := OutputSchemaFromPlan(planMap)

	for tableName, schema := range schemas {
		tableID, present := tableIDMap[tableName]
		if !present {
			return nil, fmt.Errorf("Table ID for table name %s not found in table map", tableName)
		}

		convertedRelation := AgentRelationToVizierRelation(schema)
		results = append(results, &public_vizierapipb.ExecuteScriptResponse{
			QueryID: queryID.String(),
			Result: &public_vizierapipb.ExecuteScriptResponse_MetaData{
				MetaData: &public_vizierapipb.QueryMetadata{
					Name:     tableName,
					ID:       tableID,
					Relation: convertedRelation,
				},
			},
		})
	}

	return results, nil
}
