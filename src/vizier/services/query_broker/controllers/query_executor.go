package controllers

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	planpb "pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

// QueryExecutor is responsible for handling a query's execution, from sending requests to agents, to
// tracking the responses.
type QueryExecutor struct {
	queryID         uuid.UUID
	agentList       *[]uuid.UUID
	responseByAgent []*querybrokerpb.VizierQueryResponse_ResponseByAgent
	conn            *nats.Conn
	mux             sync.Mutex
	done            chan bool
}

// NewQueryExecutor creates a Query Executor for a specific query.
func NewQueryExecutor(natsConn *nats.Conn, queryID uuid.UUID, agentList *[]uuid.UUID) Executor {
	return &QueryExecutor{
		queryID:   queryID,
		agentList: agentList,
		conn:      natsConn,
		done:      make(chan bool, 1),
	}
}

// GetQueryID gets the ID for the query that the queryExecutor is responsible for.
func (e *QueryExecutor) GetQueryID() uuid.UUID {
	return e.queryID
}

// ExecuteQuery executes a query by sending query fragments to relevant agents.
func (e *QueryExecutor) ExecuteQuery(planMap map[uuid.UUID]*planpb.Plan) error {
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

// WaitForCompletion waits until each agent has returned a result.
func (e *QueryExecutor) WaitForCompletion() ([]*querybrokerpb.VizierQueryResponse_ResponseByAgent, error) {
	// Wait for chan or timeout.
	timeout := false
	select {
	case <-e.done:
		break
	case <-time.After(1 * time.Second):
		timeout = true
	}

	if timeout {
		log.Error("Query timeout, returning available results.")
	}

	e.mux.Lock()
	defer e.mux.Unlock()

	return e.responseByAgent, nil
}

// AddResult adds a result from an agent to the list of results to be returned.
func (e *QueryExecutor) AddResult(res *querybrokerpb.AgentQueryResultRequest) {
	e.mux.Lock()
	defer e.mux.Unlock()

	e.responseByAgent = append(e.responseByAgent, &querybrokerpb.VizierQueryResponse_ResponseByAgent{
		AgentID:  res.AgentID,
		Response: res.Result,
	})

	if len(e.responseByAgent) == len(*e.agentList) {
		// Got responses from all the agents. We can send the reply.
		e.done <- true
	}
}
