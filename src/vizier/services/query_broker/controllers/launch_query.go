package controllers

import (
	"fmt"
	"strings"
	"sync"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"

	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/utils"
	messages "px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/utils/messagebus"
)

// LaunchQuery launches a query by sending query fragments to relevant agents.
func LaunchQuery(queryID uuid.UUID, natsConn *nats.Conn, planMap map[uuid.UUID]*planpb.Plan, analyze bool) error {
	if len(planMap) == 0 {
		return fmt.Errorf("Received no agent plans for query %s", queryID.String())
	}

	queryIDPB := utils.ProtoFromUUID(queryID)
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
		agentTopic := messagebus.AgentUUIDTopic(agentID)
		msgAsBytes, err := msg.Marshal()
		if err != nil {
			errs <- err
			return
		}

		err = natsConn.Publish(agentTopic, msgAsBytes)
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
