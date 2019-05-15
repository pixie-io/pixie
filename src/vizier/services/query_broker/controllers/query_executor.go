package controllers

import (
	"fmt"
	"sync"
	"time"

	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
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
func NewQueryExecutor(natsConn *nats.Conn, queryID uuid.UUID, agentList *[]uuid.UUID) *QueryExecutor {
	return &QueryExecutor{
		queryID:   queryID,
		agentList: agentList,
		conn:      natsConn,
		done:      make(chan bool, 1),
	}
}

// ExecuteQuery executes a query by sending query fragments to relevant agents.
func (e *QueryExecutor) ExecuteQuery(query string) error {
	queryIDPB, err := utils.ProtoFromUUID(&e.queryID)
	if err != nil {
		return err
	}

	// Create NATS message containing the query string.
	msg := messages.VizierMessage{
		Msg: &messages.VizierMessage_ExecuteQueryRequest{
			ExecuteQueryRequest: &messages.ExecuteQueryRequest{
				QueryID:  queryIDPB,
				QueryStr: query,
			},
		},
	}

	msgAsBytes, err := msg.Marshal()
	if err != nil {
		return err
	}

	// Accumulate failures.
	var pubErr error
	// Broadcast query to all the agents in parallel.
	var wg sync.WaitGroup
	wg.Add(len(*e.agentList))

	broadcastToAgent := func(agentID uuid.UUID) {
		defer wg.Done()
		agentTopic := fmt.Sprintf("/agent/%s", agentID.String())
		err := e.conn.Publish(agentTopic, msgAsBytes)
		if err != nil {
			pubErr = err
		}
	}

	for _, agentID := range *e.agentList {
		go broadcastToAgent(agentID)
	}

	wg.Wait()

	return err
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
