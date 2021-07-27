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

package controllers_test

import (
	"fmt"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
)

const queryIDStr = "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
const agent1ID = "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
const agent2ID = "31285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

func TestLaunchQuery(t *testing.T) {
	// Check that the query is broadcasted to all agents.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	agentUUIDStrs := [2]string{
		agent1ID,
		agent2ID,
	}

	agentUUIDs := make([]uuid.UUID, 0)
	for _, uid := range agentUUIDStrs {
		u, err := uuid.FromString(uid)
		if err != nil {
			t.Fatal(err)
		}
		agentUUIDs = append(agentUUIDs, u)
	}

	// Subscribe to each agent channel.
	sub1, err := nc.SubscribeSync(fmt.Sprintf("Agent/%s", agentUUIDStrs[0]))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	sub2, err := nc.SubscribeSync(fmt.Sprintf("Agent/%s", agentUUIDStrs[1]))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	// Plan 1 is a valid, populated plan
	plannerResultPB := &distributedpb.LogicalPlannerResult{}
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Could not unmarshal protobuf text for planner result.")
	}

	planPB1 := plannerResultPB.Plan.QbAddressToPlan[agent1ID]
	planPB2 := plannerResultPB.Plan.QbAddressToPlan[agent2ID]

	planMap := make(map[uuid.UUID]*planpb.Plan)
	planMap[agentUUIDs[0]] = planPB1
	planMap[agentUUIDs[1]] = planPB2

	// Execute a query.
	err = controllers.LaunchQuery(queryUUID, nc, planMap, false)
	require.NoError(t, err)

	// Check that each agent received the correct message.
	queryUUIDPb := utils.ProtoFromUUID(queryUUID)
	if err != nil {
		t.Fatal("Could not convert UUID to proto.")
	}
	m1, err := sub1.NextMsg(time.Second)
	require.NoError(t, err)
	pb := &messagespb.VizierMessage{}
	err = proto.Unmarshal(m1.Data, pb)
	require.NoError(t, err)

	assert.Equal(t, planPB1, pb.Msg.(*messagespb.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.Plan)
	assert.Equal(t, queryUUIDPb, pb.Msg.(*messagespb.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryID)

	m2, err := sub2.NextMsg(time.Second)
	require.NoError(t, err)
	pb = &messagespb.VizierMessage{}
	err = proto.Unmarshal(m2.Data, pb)
	require.NoError(t, err)
	assert.Equal(t, planPB2, pb.Msg.(*messagespb.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.Plan)
	assert.Equal(t, queryUUIDPb, pb.Msg.(*messagespb.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryID)
}

func TestLaunchQueryNoPlans(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	planMap := make(map[uuid.UUID]*planpb.Plan)

	err = controllers.LaunchQuery(queryUUID, nc, planMap, false)

	assert.NotNil(t, err)
	assert.Regexp(t, fmt.Sprintf("Received no agent plans for query %s", queryIDStr), err)
}

func TestLaunchQueryNATSFailure(t *testing.T) {
	// Check that the query is broadcasted to all agents.
	nc, cleanup := testingutils.MustStartTestNATS(t)

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	agentUUIDStrs := [2]string{
		agent1ID,
		agent2ID,
	}

	agentUUIDs := make([]uuid.UUID, 0)
	for _, uid := range agentUUIDStrs {
		u, err := uuid.FromString(uid)
		if err != nil {
			t.Fatal(err)
		}
		agentUUIDs = append(agentUUIDs, u)
	}

	// Plan 1 is a valid, populated plan
	plannerResultPB := &distributedpb.LogicalPlannerResult{}
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Could not unmarshal protobuf text for planner result.")
	}

	planPB1 := plannerResultPB.Plan.QbAddressToPlan[agent1ID]
	planPB2 := plannerResultPB.Plan.QbAddressToPlan[agent2ID]

	planMap := make(map[uuid.UUID]*planpb.Plan)
	planMap[agentUUIDs[0]] = planPB1
	planMap[agentUUIDs[1]] = planPB2

	// Cleanup nats before LaunchQuery, so that the Publish to NATS causes an error.
	// Previously, this would cause LaunchQuery to hang because of a bug with the error channels.
	cleanup()

	// Execute a query. This should return an error but not hang.
	err = controllers.LaunchQuery(queryUUID, nc, planMap, false)
	require.NotNil(t, err)
}
