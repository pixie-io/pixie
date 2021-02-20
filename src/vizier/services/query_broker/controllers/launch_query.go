package controllers

import (
	"fmt"
	"strings"
	"sync"

	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
)

const agentTopicPrefix = "Agent"

func getAgentTopic(agentID string) string {
	return fmt.Sprintf("%s/%s", agentTopicPrefix, agentID)
}

func getAgentTopicFromUUID(agentID uuid.UUID) string {
	return getAgentTopic(agentID.String())
}

// LaunchQuery launches a query by sending query fragments to relevant agents.
func LaunchQuery(queryID uuid.UUID, natsConn *nats.Conn, planMap map[uuid.UUID]*planpb.Plan, analyze bool) error {
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
		agentTopic := getAgentTopicFromUUID(agentID)
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
