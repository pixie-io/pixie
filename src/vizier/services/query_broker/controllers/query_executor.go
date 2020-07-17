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
	agentList   *[]uuid.UUID
	queryResult *queryresultspb.QueryResult
	// extraTables are tables get added to results as sidecars. For example, things like query plan.
	extraTables []*schemapb.Table
	// Store the compilation time so we can write it to the results later.
	compilationTime time.Duration
	conn            *nats.Conn
	mux             sync.Mutex
	done            chan bool
}

// NewQueryExecutor creates a Query Executor for a specific query.
func NewQueryExecutor(natsConn *nats.Conn, queryID uuid.UUID, agentList *[]uuid.UUID) Executor {
	return &QueryExecutor{
		queryID:     queryID,
		agentList:   agentList,
		conn:        natsConn,
		extraTables: make([]*schemapb.Table, 0),
		done:        make(chan bool, 1),
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
	for ok := true; ok; ok = hasErrs {
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
	if len(e.extraTables) > 0 {
		e.queryResult.Tables = append(e.queryResult.Tables, e.extraTables...)
	}
	// In the current implementation we just need to wait for the Kelvin node to respond.
	e.done <- true
}
