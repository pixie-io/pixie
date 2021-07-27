/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers

import (
	"fmt"

	"golang.org/x/sync/errgroup"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"

	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/utils/messagebus"
)

// LaunchQuery launches a query by sending query fragments to relevant agents.
func LaunchQuery(queryID uuid.UUID, natsConn *nats.Conn, planMap map[uuid.UUID]*planpb.Plan, analyze bool) error {
	if len(planMap) == 0 {
		return fmt.Errorf("Received no agent plans for query %s", queryID.String())
	}

	queryIDPB := utils.ProtoFromUUID(queryID)
	// Broadcast query to all the agents in parallel.
	var eg errgroup.Group

	broadcastToAgent := func(agentID uuid.UUID, logicalPlan *planpb.Plan) error {
		// Create NATS message containing the query string.
		msg := messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_ExecuteQueryRequest{
				ExecuteQueryRequest: &messagespb.ExecuteQueryRequest{
					QueryID: queryIDPB,
					Plan:    logicalPlan,
					Analyze: analyze,
				},
			},
		}
		agentTopic := messagebus.AgentUUIDTopic(agentID)
		msgAsBytes, err := msg.Marshal()
		if err != nil {
			return err
		}

		err = natsConn.Publish(agentTopic, msgAsBytes)
		if err != nil {
			return err
		}
		return nil
	}

	for agentID, logicalPlan := range planMap {
		agentID := agentID
		logicalPlan := logicalPlan
		eg.Go(func() error {
			return broadcastToAgent(agentID, logicalPlan)
		})
	}

	if err := eg.Wait(); err != nil {
		return err
	}

	return nil
}
