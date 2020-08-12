package controllers

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/carnotpb"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/table_store/proto/types"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

// QueryExecutor is responsible for handling a query's execution, from sending requests to agents, to
// tracking the responses.
type QueryExecutor struct {
	queryID     uuid.UUID
	queryResult *queryresultspb.QueryResult
	conn        *nats.Conn
	mux         sync.Mutex
	done        chan bool

	// temporary variables used to support the streaming TransferResultChunk API
	// simultaneously with ReceiveAgentQueryResult. TODO(nserrino): Remove and refactor when
	// Kelvin has fully moved to the TransferResultChunk API.
	schema              map[string]*schemapb.Relation
	rowBatches          map[string][]*schemapb.RowBatchData
	gotEOS              map[string]bool
	executionStats      *queryresultspb.QueryExecutionStats
	agentExecutionStats []*queryresultspb.AgentExecutionStats
}

// NewQueryExecutor creates a Query Executor for a specific query.
func NewQueryExecutor(natsConn *nats.Conn, queryID uuid.UUID) Executor {
	return &QueryExecutor{
		queryID:    queryID,
		conn:       natsConn,
		done:       make(chan bool, 1),
		gotEOS:     make(map[string]bool),
		rowBatches: make(map[string][]*schemapb.RowBatchData),
	}
}

// GetQueryID gets the ID for the query that the queryExecutor is responsible for.
func (e *QueryExecutor) GetQueryID() uuid.UUID {
	return e.queryID
}

// AddQueryPlanToResult adds the query plan to the list of output tables.
func AddQueryPlanToResult(queryResult *queryresultspb.QueryResult, plan *distributedpb.DistributedPlan, planMap map[uuid.UUID]*planpb.Plan, execStats *[]*queryresultspb.AgentExecutionStats) error {
	queryPlan, err := getQueryPlanAsDotString(plan, planMap, execStats)
	if err != nil {
		log.WithError(err).Error("error with query plan")
	}

	var extraTables []*schemapb.Table
	extraTables = append(extraTables, &schemapb.Table{
		Relation: &schemapb.Relation{Columns: []*schemapb.Relation_ColumnInfo{
			{
				ColumnName: "query_plan",
				ColumnType: typespb.STRING,
				ColumnDesc: "The query plan",
			},
		}},
		Name: "__query_plan__",
		RowBatches: []*schemapb.RowBatchData{
			{
				Cols: []*schemapb.Column{
					{
						ColData: &schemapb.Column_StringData{
							StringData: &schemapb.StringColumn{
								Data: []types.StringData{
									[]byte(queryPlan),
								},
							},
						},
					},
				},
				NumRows: 1,
				Eos:     true,
				Eow:     true,
			},
		},
	})

	queryResult.Tables = append(queryResult.Tables, extraTables...)
	return nil
}

// ExecuteQuery executes a query by sending query fragments to relevant agents.
func (e *QueryExecutor) ExecuteQuery(planMap map[uuid.UUID]*planpb.Plan, analyze bool) error {
	queryIDPB := utils.ProtoFromUUID(&e.queryID)
	// Accumulate failures.
	errs := make(chan error)
	defer close(errs)
	// Broadcast query to all the agents in parallel.
	var wg sync.WaitGroup
	wg.Add(len(planMap))

	broadcastToAgent := func(agentID uuid.UUID, logicalPlan *planpb.Plan) {
		defer wg.Done()
		// Create NATS message containing the query string.
		msg := messages.VizierMessage{
			Msg: &messages.VizierMessage_ExecuteQueryRequest{
				ExecuteQueryRequest: &messages.ExecuteQueryRequest{
					QueryID: queryIDPB,
					Plan:    logicalPlan,
					Analyze: analyze,
				},
			},
		}
		agentTopic := fmt.Sprintf("/agent/%s", agentID.String())

		msgAsBytes, err := msg.Marshal()
		if err != nil {
			errs <- err
			return
		}

		err = e.conn.Publish(agentTopic, msgAsBytes)
		if err != nil {
			errs <- err
			return
		}
	}

	for agentID, logicalPlan := range planMap {
		go broadcastToAgent(agentID, logicalPlan)
	}

	wg.Wait()
	errsList := make([]string, 0)

	hasErrs := true
	for hasErrs {
		select {
		case err := <-errs:
			errsList = append(errsList, err.Error())
		default:
			hasErrs = false
		}
	}

	if len(errsList) > 0 {
		return fmt.Errorf(strings.Join(errsList, "\n"))
	}

	return nil
}

// WaitForCompletion waits until we have received the results (or timeout).
func (e *QueryExecutor) WaitForCompletion() (*queryresultspb.QueryResult, error) {
	// Wait for chan or timeout.
	timeout := false
	select {
	case <-e.done:
		break
	case <-time.After(6 * time.Second):
		timeout = true
	}

	if timeout {
		log.Error("Query timeout, returning available results.")
	}

	e.mux.Lock()
	defer e.mux.Unlock()

	return e.queryResult, nil
}

// AddResult adds a result from an agent to the list of results to be returned.
func (e *QueryExecutor) AddResult(res *querybrokerpb.AgentQueryResultRequest) {
	e.mux.Lock()
	defer e.mux.Unlock()

	if e.queryResult != nil {
		log.
			WithField("query_id", e.queryID.String()).
			Error("Internal error: already have response for this query")
		return
	}
	e.queryResult = res.Result.QueryResult
	// In the current implementation we just need to wait for the Kelvin node to respond.
	e.done <- true
}

// Temorary glue code used to signal WaitForCompletion() that the results from the new streaming
// API, TransferResultChunk, have all arrived successfully.
func (e *QueryExecutor) signalDoneIfStreamingComplete() {
	if e.schema == nil {
		return
	}
	if e.executionStats == nil {
		return
	}
	if e.agentExecutionStats == nil {
		return
	}
	if len(e.gotEOS) < len(e.schema) {
		return
	}

	allEOS := true
	for tableName := range e.schema {
		if eos, present := e.gotEOS[tableName]; !eos || !present {
			allEOS = false
			break
		}
	}
	// If all of the tables have sent their EOS message, and we have received all of the metadata
	// for the query, then we can signal `done` to WaitForCompletion().
	if allEOS {
		e.buildStreamedResult()
		e.done <- true
	}
}

// When we got all of our results from TransferResultChunk, build the QueryResult message
// from their contents. TODO(nserrino): Remove and refactor when batch API is deprecated.
func (e *QueryExecutor) buildStreamedResult() {
	var tables []*schemapb.Table
	for tableName, relation := range e.schema {
		table := &schemapb.Table{
			Name:     tableName,
			Relation: relation,
		}
		if rowBatches, present := e.rowBatches[tableName]; present {
			table.RowBatches = rowBatches
		}
		tables = append(tables, table)
	}

	e.queryResult = &queryresultspb.QueryResult{
		Tables:              tables,
		TimingInfo:          e.executionStats.Timing,
		ExecutionStats:      e.executionStats,
		AgentExecutionStats: e.agentExecutionStats,
	}
}

// AddStreamedResult adds a streamed result from an agent to the list of results to be returned.
// TODO(nserrino): This is temporary glue code as we move Kelvin from a batch API to a streaming
// API. Remove this and centralize result handling logic in query_result_forwarder when the transition
// to streaming is complete on Kelvin.
func (e *QueryExecutor) AddStreamedResult(res *carnotpb.TransferResultChunkRequest) error {
	e.mux.Lock()
	defer e.mux.Unlock()

	msgQueryID := utils.UUIDFromProtoOrNil(res.QueryID)
	if msgQueryID != e.queryID {
		return fmt.Errorf("Error in AddStreamedResult: msg query ID %s doesn't match executor ID %s",
			msgQueryID.String(), e.queryID.String())
	}

	if schema := res.GetSchema(); schema != nil {
		e.schema = schema.RelationMap
	}
	if execStats := res.GetExecutionAndTimingInfo(); execStats != nil {
		e.executionStats = execStats.ExecutionStats
		e.agentExecutionStats = execStats.AgentExecutionStats
	}
	if rbResult := res.GetRowBatchResult(); rbResult != nil {
		tableName := rbResult.GetTableName()
		if tableName == "" {
			return fmt.Errorf("Error in AddStreamedResult: ResultRowBatch does not contain table name")
		}
		e.rowBatches[tableName] = append(e.rowBatches[tableName], rbResult.GetRowBatch())
		if rbResult.GetRowBatch().Eos {
			e.gotEOS[tableName] = true
		}
	}

	e.signalDoneIfStreamingComplete()
	return nil
}
